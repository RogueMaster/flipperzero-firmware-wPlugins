from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from collections.abc import Sequence
from pathlib import Path

from scripts.fibp_codec import (
    ErrorCode,
    Frame,
    MessageType,
    StreamDecoder,
    encode_frame,
)

from . import __version__
from .network import NetworkPolicyError, NetworkRequestError, perform_request
from .permissions import PermissionStore
from .serial_ports import open_serial, serial_candidates
from .session import (
    CompletedRequest,
    HostSession,
    PermissionDecision,
    response_end_frame,
    response_header_frames,
    response_start_frame,
)


class BridgeHost:
    def __init__(
        self,
        permission_store: PermissionStore,
        automatic_permission: PermissionDecision | None,
        verbose: bool,
    ):
        self.permission_store = permission_store
        self.automatic_permission = automatic_permission
        self.verbose = verbose
        self.outgoing: queue.Queue[Frame] = queue.Queue(maxsize=256)
        self.session = HostSession(self._start_request, permission_store.contains)
        self._network_threads: set[threading.Thread] = set()

    def log(self, message: str) -> None:
        print(message, file=sys.stderr, flush=True)

    def debug(self, message: str) -> None:
        if self.verbose:
            self.log(message)

    def _put(self, frame: Frame) -> None:
        try:
            self.outgoing.put(frame, timeout=2.0)
        except queue.Full:
            if self.session.active:
                self.session.active.cancel.set()
            self.log("Outgoing USB queue is full; request cancelled")

    def _start_request(self, request: CompletedRequest) -> None:
        worker = threading.Thread(
            target=self._perform_request,
            args=(request,),
            name=f"fib-request-{request.request_id}",
            daemon=True,
        )
        self._network_threads.add(worker)
        worker.start()

    def _perform_request(self, request: CompletedRequest) -> None:
        sequence = 0
        started = False
        sent = 0

        def on_start(status: int, headers, declared: int) -> None:
            nonlocal sequence, started
            self._put(
                response_start_frame(request.request_id, status, headers, declared)
            )
            for frame in response_header_frames(request.request_id, headers):
                self._put(frame)
            sequence = 1 + len(headers)
            started = True

        def on_chunk(chunk: bytes) -> None:
            nonlocal sequence, sent
            self._put(
                Frame(
                    MessageType.RESPONSE_BODY_CHUNK,
                    chunk,
                    request_id=request.request_id,
                    sequence=sequence,
                )
            )
            sequence += 1
            sent += len(chunk)

        try:
            result = perform_request(
                request.start,
                list(request.headers),
                request.body,
                request.maximum_response_bytes,
                request.cancel,
                on_start,
                on_chunk,
            )
            if started:
                outcome = 2 if result.cancelled else (1 if result.truncated else 0)
                self._put(
                    response_end_frame(
                        request.request_id, sequence, outcome, result.bytes_sent
                    )
                )
            self.debug(
                f"request {request.request_id} completed: {result.bytes_sent} bytes"
            )
        except NetworkPolicyError as error:
            self._put(
                self.session.request_error(
                    request.request_id, ErrorCode.SECURITY_BLOCKED, str(error)
                )
            )
        except TimeoutError as error:
            self._put(
                self.session.request_error(
                    request.request_id, ErrorCode.TIMEOUT, str(error)
                )
            )
        except (NetworkRequestError, OSError, ValueError) as error:
            self._put(
                self.session.request_error(
                    request.request_id, ErrorCode.NETWORK_FAILURE, str(error)
                )
            )
        except (
            Exception
        ) as error:  # noqa: BLE001 - a request must never terminate the host
            self._put(
                self.session.request_error(
                    request.request_id, ErrorCode.INTERNAL_ERROR, type(error).__name__
                )
            )
        finally:
            self.session.finish_request(request.request_id)
            self._network_threads.discard(threading.current_thread())

    def resolve_permission(self) -> list[Frame]:
        hello = self.session.hello
        if not hello or not self.session.permission_pending:
            return []
        decision = self.automatic_permission
        if decision is None:
            print(
                f"\n{hello.name} ({hello.model}, ID …{hello.device_id.hex()[-8:]}) wants "
                "to use this computer's internet connection.\n"
                "The Wi-Fi password and browser cookies are not shared.",
                flush=True,
            )
            while True:
                answer = (
                    input("[d] Deny  [o] Allow once  [a] Always allow: ")
                    .strip()
                    .lower()
                )
                if answer in {"d", "deny"}:
                    decision = PermissionDecision.DENY
                    break
                if answer in {"o", "once"}:
                    decision = PermissionDecision.ALLOW_ONCE
                    break
                if answer in {"a", "always"}:
                    decision = PermissionDecision.ALLOW_ALWAYS
                    break
        if decision == PermissionDecision.ALLOW_ALWAYS:
            self.permission_store.grant(hello)
        return self.session.resolve_permission(decision or PermissionDecision.DENY)

    def close(self) -> None:
        self.session.close()
        for worker in tuple(self._network_threads):
            worker.join(timeout=1.5)


def _write_frames(connection, frames: Sequence[Frame], verbose: bool) -> None:
    for frame in frames:
        if verbose:
            print(
                f"TX {frame.type_name} req={frame.request_id} seq={frame.sequence}",
                file=sys.stderr,
            )
        connection.write(encode_frame(frame))
    connection.flush()


def serve_port(device: str, host: BridgeHost, handshake_timeout: float = 8.0) -> bool:
    print(f"Opening {device}", file=sys.stderr, flush=True)
    connection = open_serial(device)
    decoder = StreamDecoder(
        lambda issue: host.debug(f"parser: {issue.code}: {issue.detail}")
    )
    connected_at = time.monotonic()
    handshaken = False
    try:
        while True:
            while True:
                try:
                    frame = host.outgoing.get_nowait()
                except queue.Empty:
                    break
                _write_frames(connection, [frame], host.verbose)

            data = connection.read(4096)
            if not data:
                if (
                    not handshaken
                    and time.monotonic() - connected_at >= handshake_timeout
                ):
                    return False
                continue
            for frame in decoder.feed(data):
                host.debug(
                    f"RX {frame.type_name} req={frame.request_id} seq={frame.sequence}"
                )
                responses = host.session.on_frame(frame)
                if frame.message_type == MessageType.HELLO:
                    handshaken = host.session.hello is not None
                    if handshaken:
                        print(
                            f"Connected to {host.session.hello.name}; FIBP 1.0",
                            file=sys.stderr,
                            flush=True,
                        )
                _write_frames(connection, responses, host.verbose)
                if host.session.permission_pending:
                    _write_frames(connection, host.resolve_permission(), host.verbose)
    finally:
        host.close()
        connection.close()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="fib-bridge",
        description="Use this computer's HTTPS connection from FIBP-enabled Flipper apps.",
    )
    parser.add_argument(
        "--port", help="serial port; omit for automatic Flipper discovery"
    )
    permission = parser.add_mutually_exclusive_group()
    permission.add_argument(
        "--allow-once",
        action="store_true",
        help="allow the next device without prompting",
    )
    permission.add_argument(
        "--deny", action="store_true", help="deny the next device without prompting"
    )
    parser.add_argument(
        "--permissions-file", type=Path, help="override persistent permission storage"
    )
    parser.add_argument(
        "--list-ports", action="store_true", help="list serial devices and exit"
    )
    parser.add_argument(
        "--once", action="store_true", help="exit after the selected port disconnects"
    )
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument(
        "--version", action="version", version=f"%(prog)s {__version__}"
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    candidates = serial_candidates()
    if args.list_ports:
        for candidate in candidates:
            marker = "Flipper" if candidate.likely_flipper else "serial"
            print(f"{candidate.device}\t{marker}\t{candidate.description}")
        return 0

    automatic = (
        PermissionDecision.ALLOW_ONCE
        if args.allow_once
        else (PermissionDecision.DENY if args.deny else None)
    )
    store = PermissionStore(args.permissions_file)
    while True:
        devices = (
            [args.port]
            if args.port
            else [item.device for item in serial_candidates() if item.likely_flipper]
        )
        if not devices and not args.port:
            print("Waiting for a Flipper Zero…", file=sys.stderr, flush=True)
            time.sleep(1.0)
            continue
        for device in devices:
            host = BridgeHost(store, automatic, args.verbose)
            try:
                served = serve_port(device, host)
                if served or args.once:
                    return 0
            except (OSError, RuntimeError, EOFError, KeyboardInterrupt) as error:
                host.close()
                if isinstance(error, KeyboardInterrupt):
                    return 130
                print(f"{device}: {error}", file=sys.stderr, flush=True)
                if args.port or args.once:
                    return 1
        time.sleep(1.0)


if __name__ == "__main__":
    raise SystemExit(main())
