#!/usr/bin/env python3
"""PTY-based, hardware-free FIBP v1 peer simulator.

The simulator owns the PTY master and prints the slave path on stdout.  Point a
serial client at that slave path.  Diagnostics go to stderr so scripts can read
the first stdout line without parsing log text.
"""

from __future__ import annotations

import argparse
import errno
import os
import pty
import secrets
import select
import signal
import struct
import sys
import time
import tty
from dataclasses import dataclass, field
from threading import Event
from typing import Callable, Sequence

try:
    from .fibp_codec import (
        Capability,
        ErrorCode,
        Frame,
        FrameFlags,
        HeaderField,
        Hello,
        HelloAck,
        MAX_FRAME_SIZE,
        MAX_RESPONSE_BODY_BYTES,
        MessageType,
        PayloadDecodeError,
        RequestStart,
        ResponseEnd,
        ResponseStart,
        StreamDecoder,
        decode_header,
        decode_hello,
        decode_hello_ack,
        decode_permission_status,
        decode_ping_token,
        decode_request_start,
        decode_response_end,
        decode_response_start,
        encode_frame,
        encode_header,
        encode_hello,
        encode_hello_ack,
        encode_permission_status,
        encode_ping_token,
        encode_request_start,
        encode_response_end,
        encode_response_start,
        fragment_bytes,
    )
except ImportError:  # Direct execution: python3 scripts/fibp_simulator.py
    from fibp_codec import (  # type: ignore
        Capability,
        ErrorCode,
        Frame,
        FrameFlags,
        HeaderField,
        Hello,
        HelloAck,
        MAX_FRAME_SIZE,
        MAX_RESPONSE_BODY_BYTES,
        MessageType,
        PayloadDecodeError,
        RequestStart,
        ResponseEnd,
        ResponseStart,
        StreamDecoder,
        decode_header,
        decode_hello,
        decode_hello_ack,
        decode_permission_status,
        decode_ping_token,
        decode_request_start,
        decode_response_end,
        decode_response_start,
        encode_frame,
        encode_header,
        encode_hello,
        encode_hello_ack,
        encode_permission_status,
        encode_ping_token,
        encode_request_start,
        encode_response_end,
        encode_response_start,
        fragment_bytes,
    )


DEFAULT_CAPABILITIES = int(
    Capability.HTTPS_GET
    | Capability.HTTPS_POST
    | Capability.REQUEST_HEADERS
    | Capability.RESPONSE_HEADERS
    | Capability.CANCELLATION
)
DEFAULT_REQUEST_ID = 0x10203040
DEFAULT_CLIENT_NONCE = 0x0102030405060708
DEFAULT_SERVER_NONCE = 0xA1A2A3A4A5A6A7A8
DEFAULT_PING_TOKEN = 0x8877665544332211


LogFunction = Callable[[str], None]


def _no_log(_message: str) -> None:
    return


def _encode_error_payload(
    code: ErrorCode,
    scope: int,
    offending_type: int,
    detail: str,
) -> bytes:
    encoded = detail.encode("utf-8", errors="replace")[:128]
    return (
        struct.pack("<HBBH", int(code), scope, offending_type & 0xFF, len(encoded))
        + encoded
    )


def _error_frame(
    code: ErrorCode,
    offending: Frame,
    detail: str,
    *,
    request_scope: bool = True,
    sequence: int = 0,
) -> Frame:
    return Frame(
        MessageType.ERROR,
        _encode_error_payload(
            code, 1 if request_scope else 0, offending.message_type, detail
        ),
        request_id=offending.request_id if request_scope else 0,
        sequence=sequence,
    )


@dataclass
class PendingRequest:
    request_id: int
    start: RequestStart
    expected_sequence: int = 1
    headers: list[HeaderField] = field(default_factory=list)
    body: bytearray = field(default_factory=bytearray)
    aggregate_header_bytes: int = 0


class MacRoleEngine:
    """Deterministic user-space helper behavior without making a real request."""

    def __init__(
        self,
        *,
        response_text: str = "FIBP simulator HTTPS response\n",
        server_nonce: int = DEFAULT_SERVER_NONCE,
        log: LogFunction = _no_log,
    ):
        self.response_text = response_text
        self.server_nonce = server_nonce
        self.log = log
        self.hello: Hello | None = None
        self.handshaken = False
        self.permission_granted = False
        self.control_tx_sequence = 0
        self.pending: PendingRequest | None = None
        self.complete = False
        self.last_ping_token: int | None = None

    def _hello_responses(self, hello: Hello) -> list[Frame]:
        negotiated_capabilities = hello.capabilities & DEFAULT_CAPABILITIES
        ack = HelloAck(
            selected_major=1,
            selected_minor=0,
            capabilities=negotiated_capabilities,
            maximum_payload=min(hello.maximum_rx_payload, 512),
            maximum_response_bytes=min(
                hello.maximum_response_bytes, MAX_RESPONSE_BODY_BYTES
            ),
            echoed_client_nonce=hello.client_nonce,
            server_nonce=self.server_nonce,
        )
        self.control_tx_sequence = 2
        self.handshaken = True
        self.permission_granted = True
        return [
            Frame(MessageType.HELLO_ACK, encode_hello_ack(ack), sequence=0),
            Frame(
                MessageType.PERMISSION_STATUS,
                encode_permission_status(1, 0),
                sequence=1,
            ),
        ]

    def _response_frames(self, pending: PendingRequest) -> list[Frame]:
        full_body = self.response_text.encode("utf-8")
        truncated = len(full_body) > MAX_RESPONSE_BODY_BYTES
        body = full_body[:MAX_RESPONSE_BODY_BYTES]
        content_type = HeaderField("content-type", "text/plain; charset=utf-8")
        frames = [
            Frame(
                MessageType.RESPONSE_START,
                encode_response_start(ResponseStart(200, 1, len(body))),
                request_id=pending.request_id,
                sequence=0,
            ),
            Frame(
                MessageType.RESPONSE_HEADER,
                encode_header(content_type),
                request_id=pending.request_id,
                sequence=1,
            ),
        ]
        sequence = 2
        for offset in range(0, len(body), 192):
            frames.append(
                Frame(
                    MessageType.RESPONSE_BODY_CHUNK,
                    body[offset : offset + 192],
                    request_id=pending.request_id,
                    sequence=sequence,
                )
            )
            sequence += 1
        frames.append(
            Frame(
                MessageType.RESPONSE_END,
                encode_response_end(ResponseEnd(1 if truncated else 0, len(body))),
                request_id=pending.request_id,
                sequence=sequence,
                flags=FrameFlags.FINAL
                | (FrameFlags.TRUNCATED if truncated else FrameFlags.NONE),
            )
        )
        return frames

    def on_frame(self, frame: Frame) -> list[Frame]:
        self.log(f"RX {frame.type_name} req={frame.request_id} seq={frame.sequence}")
        try:
            message_type = MessageType(frame.message_type)
        except ValueError:
            return [
                _error_frame(
                    ErrorCode.UNSUPPORTED_MESSAGE, frame, "unknown message type"
                )
            ]

        try:
            if message_type == MessageType.HELLO:
                if frame.request_id != 0 or frame.sequence != 0:
                    return [
                        _error_frame(
                            ErrorCode.INVALID_STATE,
                            frame,
                            "HELLO requires request=0 sequence=0",
                            request_scope=False,
                        )
                    ]
                hello = decode_hello(frame.payload)
                if not (
                    hello.minimum_major <= 1 <= hello.maximum_major
                    and hello.minimum_minor <= 0 <= hello.maximum_minor
                ):
                    return [
                        _error_frame(
                            ErrorCode.UNSUPPORTED_VERSION,
                            frame,
                            "no common FIBP version",
                            request_scope=False,
                        )
                    ]
                if (
                    self.hello is not None
                    and self.hello.client_nonce != hello.client_nonce
                ):
                    self.pending = None
                    self.complete = False
                self.hello = hello
                return self._hello_responses(hello)

            if not self.handshaken:
                return [
                    _error_frame(
                        ErrorCode.INVALID_STATE,
                        frame,
                        "HELLO is required first",
                        request_scope=False,
                    )
                ]

            if message_type == MessageType.PING:
                token = decode_ping_token(frame.payload)
                self.last_ping_token = token
                response = Frame(
                    MessageType.PONG,
                    encode_ping_token(token),
                    sequence=self.control_tx_sequence,
                )
                self.control_tx_sequence += 1
                return [response]

            if message_type == MessageType.PONG:
                self.last_ping_token = decode_ping_token(frame.payload)
                return []

            if message_type == MessageType.REQUEST_START:
                if not self.permission_granted:
                    return [
                        _error_frame(
                            ErrorCode.PERMISSION_DENIED,
                            frame,
                            "permission is not granted",
                        )
                    ]
                if (
                    frame.request_id == 0
                    or frame.sequence != 0
                    or self.pending is not None
                ):
                    return [
                        _error_frame(
                            ErrorCode.INVALID_STATE, frame, "request cannot start"
                        )
                    ]
                self.pending = PendingRequest(
                    frame.request_id, decode_request_start(frame.payload)
                )
                return []

            if message_type in (
                MessageType.REQUEST_HEADER,
                MessageType.REQUEST_BODY_CHUNK,
                MessageType.REQUEST_END,
            ):
                pending = self.pending
                if pending is None or pending.request_id != frame.request_id:
                    return [
                        _error_frame(
                            ErrorCode.INVALID_STATE, frame, "no matching request"
                        )
                    ]
                if frame.sequence != pending.expected_sequence:
                    code = (
                        ErrorCode.DUPLICATE_SEQUENCE
                        if frame.sequence < pending.expected_sequence
                        else ErrorCode.SEQUENCE_GAP
                    )
                    self.pending = None
                    return [_error_frame(code, frame, "unexpected request sequence")]
                pending.expected_sequence += 1

                if message_type == MessageType.REQUEST_HEADER:
                    header = decode_header(frame.payload)
                    pending.headers.append(header)
                    pending.aggregate_header_bytes += len(
                        header.name.encode("utf-8")
                    ) + len(header.value.encode("utf-8"))
                    if len(pending.headers) > pending.start.declared_header_count:
                        self.pending = None
                        return [
                            _error_frame(
                                ErrorCode.INVALID_REQUEST, frame, "too many headers"
                            )
                        ]
                    if pending.aggregate_header_bytes > 1024:
                        self.pending = None
                        return [
                            _error_frame(
                                ErrorCode.INVALID_REQUEST,
                                frame,
                                "aggregate headers exceed 1024 bytes",
                            )
                        ]
                    return []

                if message_type == MessageType.REQUEST_BODY_CHUNK:
                    pending.body.extend(frame.payload)
                    if len(pending.body) > pending.start.declared_body_length:
                        self.pending = None
                        return [
                            _error_frame(
                                ErrorCode.INVALID_REQUEST,
                                frame,
                                "body exceeds declaration",
                            )
                        ]
                    return []

                if frame.payload or not (frame.flags & FrameFlags.FINAL):
                    self.pending = None
                    return [
                        _error_frame(
                            ErrorCode.INVALID_REQUEST, frame, "invalid REQUEST_END"
                        )
                    ]
                if (
                    len(pending.headers) != pending.start.declared_header_count
                    or len(pending.body) != pending.start.declared_body_length
                ):
                    self.pending = None
                    return [
                        _error_frame(
                            ErrorCode.INVALID_REQUEST, frame, "request is incomplete"
                        )
                    ]
                responses = self._response_frames(pending)
                self.pending = None
                self.complete = True
                return responses

            if message_type == MessageType.CANCEL:
                if (
                    self.pending is not None
                    and self.pending.request_id == frame.request_id
                ):
                    self.pending = None
                return []

            if message_type in (MessageType.ERROR, MessageType.DISCONNECT):
                return []

            return [
                _error_frame(
                    ErrorCode.INVALID_STATE, frame, "message has wrong direction"
                )
            ]
        except (PayloadDecodeError, ValueError) as error:
            return [_error_frame(ErrorCode.INVALID_REQUEST, frame, str(error))]


class FlipperRoleEngine:
    """A small Flipper-side demo client for exercising a desktop host."""

    def __init__(
        self,
        *,
        request_url: str = "https://example.com/fibp-demo",
        client_nonce: int = DEFAULT_CLIENT_NONCE,
        request_id: int = DEFAULT_REQUEST_ID,
        ping_token: int = DEFAULT_PING_TOKEN,
        auto_request: bool = True,
        log: LogFunction = _no_log,
    ):
        self.request_url = request_url
        self.client_nonce = client_nonce
        self.request_id = request_id
        self.ping_token = ping_token
        self.auto_request = auto_request
        self.log = log
        self.handshaken = False
        self.ready = False
        self.request_sent = False
        self.control_tx_sequence = 1
        self.response_expected_sequence = 0
        self.response_started: ResponseStart | None = None
        self.response_headers: list[HeaderField] = []
        self.response_body = bytearray()
        self.response_end: ResponseEnd | None = None
        self.pong_received = False
        self.complete = False

    def initial_frames(self) -> list[Frame]:
        hello = Hello(
            minimum_major=1,
            minimum_minor=0,
            maximum_major=1,
            maximum_minor=0,
            capabilities=DEFAULT_CAPABILITIES,
            maximum_rx_payload=512,
            maximum_response_bytes=MAX_RESPONSE_BODY_BYTES,
            client_nonce=self.client_nonce,
            model="Flipper Zero",
            name="FIBP Simulator",
            id_type=1,
            device_id=bytes.fromhex("0102030405060708"),
            app_version="0.3",
        )
        return [Frame(MessageType.HELLO, encode_hello(hello), sequence=0)]

    def _example_request(self) -> list[Frame]:
        start = RequestStart(
            method=1,
            timeout_ms=15000,
            url=self.request_url,
            declared_body_length=0,
            declared_header_count=1,
        )
        return [
            Frame(
                MessageType.REQUEST_START,
                encode_request_start(start),
                request_id=self.request_id,
                sequence=0,
            ),
            Frame(
                MessageType.REQUEST_HEADER,
                encode_header(HeaderField("accept", "text/plain")),
                request_id=self.request_id,
                sequence=1,
            ),
            Frame(
                MessageType.REQUEST_END,
                request_id=self.request_id,
                sequence=2,
                flags=FrameFlags.FINAL,
            ),
        ]

    def on_frame(self, frame: Frame) -> list[Frame]:
        self.log(f"RX {frame.type_name} req={frame.request_id} seq={frame.sequence}")
        try:
            message_type = MessageType(frame.message_type)
        except ValueError:
            return [
                _error_frame(
                    ErrorCode.UNSUPPORTED_MESSAGE, frame, "unknown message type"
                )
            ]

        try:
            if message_type == MessageType.HELLO_ACK:
                ack = decode_hello_ack(frame.payload)
                if frame.request_id != 0 or frame.sequence != 0:
                    raise PayloadDecodeError("HELLO_ACK requires request=0 sequence=0")
                if ack.echoed_client_nonce != self.client_nonce:
                    raise PayloadDecodeError("HELLO_ACK nonce mismatch")
                if (ack.selected_major, ack.selected_minor) != (1, 0):
                    raise PayloadDecodeError(
                        "HELLO_ACK selected an unsupported version"
                    )
                self.handshaken = True
                return []

            if message_type == MessageType.PERMISSION_STATUS:
                state, _reason = decode_permission_status(frame.payload)
                if not self.handshaken:
                    raise PayloadDecodeError("permission arrived before HELLO_ACK")
                self.ready = state in (1, 2)
                if not self.ready or self.request_sent:
                    return []
                self.request_sent = True
                outgoing = [
                    Frame(
                        MessageType.PING,
                        encode_ping_token(self.ping_token),
                        sequence=self.control_tx_sequence,
                    )
                ]
                self.control_tx_sequence += 1
                if self.auto_request:
                    outgoing.extend(self._example_request())
                return outgoing

            if message_type == MessageType.PERMISSION_REQUIRED:
                return []

            if message_type == MessageType.PING:
                token = decode_ping_token(frame.payload)
                response = Frame(
                    MessageType.PONG,
                    encode_ping_token(token),
                    sequence=self.control_tx_sequence,
                )
                self.control_tx_sequence += 1
                return [response]

            if message_type == MessageType.PONG:
                self.pong_received = decode_ping_token(frame.payload) == self.ping_token
                if self.pong_received and not self.auto_request:
                    self.complete = True
                return []

            if message_type in (
                MessageType.RESPONSE_START,
                MessageType.RESPONSE_HEADER,
                MessageType.RESPONSE_BODY_CHUNK,
                MessageType.RESPONSE_END,
            ):
                if frame.request_id != self.request_id:
                    return [
                        _error_frame(
                            ErrorCode.INVALID_STATE, frame, "wrong response request ID"
                        )
                    ]
                if frame.sequence != self.response_expected_sequence:
                    code = (
                        ErrorCode.DUPLICATE_SEQUENCE
                        if frame.sequence < self.response_expected_sequence
                        else ErrorCode.SEQUENCE_GAP
                    )
                    return [_error_frame(code, frame, "unexpected response sequence")]
                self.response_expected_sequence += 1

                if message_type == MessageType.RESPONSE_START:
                    if self.response_started is not None:
                        raise PayloadDecodeError("duplicate RESPONSE_START")
                    self.response_started = decode_response_start(frame.payload)
                    return []
                if self.response_started is None:
                    raise PayloadDecodeError(
                        "response data arrived before RESPONSE_START"
                    )
                if message_type == MessageType.RESPONSE_HEADER:
                    self.response_headers.append(decode_header(frame.payload))
                    return []
                if message_type == MessageType.RESPONSE_BODY_CHUNK:
                    if len(frame.payload) > 192:
                        raise PayloadDecodeError("response chunk exceeds 192 bytes")
                    self.response_body.extend(frame.payload)
                    if len(self.response_body) > MAX_RESPONSE_BODY_BYTES:
                        raise PayloadDecodeError("response exceeds 4 MiB")
                    return []

                if not (frame.flags & FrameFlags.FINAL):
                    raise PayloadDecodeError("RESPONSE_END is missing FINAL")
                self.response_end = decode_response_end(frame.payload)
                if self.response_end.bytes_sent != len(self.response_body):
                    raise PayloadDecodeError("RESPONSE_END byte count mismatch")
                if (
                    len(self.response_headers)
                    != self.response_started.declared_header_count
                ):
                    raise PayloadDecodeError("response header count mismatch")
                if (
                    self.response_started.declared_body_length != 0xFFFFFFFF
                    and self.response_started.declared_body_length
                    != len(self.response_body)
                ):
                    raise PayloadDecodeError("response body length mismatch")
                self.complete = True
                return []

            if message_type in (MessageType.ERROR, MessageType.DISCONNECT):
                self.complete = True
                return []

            return [
                _error_frame(
                    ErrorCode.INVALID_STATE, frame, "message has wrong direction"
                )
            ]
        except (PayloadDecodeError, ValueError) as error:
            return [_error_frame(ErrorCode.INVALID_REQUEST, frame, str(error))]


@dataclass
class PseudoTerminal:
    master_fd: int
    slave_fd: int
    slave_path: str

    @classmethod
    def open(cls) -> "PseudoTerminal":
        master_fd, slave_fd = pty.openpty()
        tty.setraw(slave_fd)
        os.set_blocking(master_fd, False)
        return cls(master_fd, slave_fd, os.ttyname(slave_fd))

    def close(self) -> None:
        for descriptor in (self.master_fd, self.slave_fd):
            try:
                os.close(descriptor)
            except OSError:
                pass

    def __enter__(self) -> "PseudoTerminal":
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()


class FrameTransmitter:
    def __init__(
        self,
        fragment_sizes: Sequence[int],
        *,
        fragment_delay_seconds: float = 0,
        corrupt_crc: str = "none",
        corrupt_message: MessageType | None = None,
        log: LogFunction = _no_log,
    ):
        if not fragment_sizes or any(size <= 0 for size in fragment_sizes):
            raise ValueError("fragment sizes must be positive")
        self.fragment_sizes = tuple(fragment_sizes)
        self.fragment_delay_seconds = fragment_delay_seconds
        self.corrupt_crc = corrupt_crc
        self.corrupt_message = corrupt_message
        self.log = log
        self.corruption_used = False

    def encoded(self, frame: Frame) -> bytes:
        encoded = bytearray(encode_frame(frame))
        matches = (
            self.corrupt_message is None or frame.message_type == self.corrupt_message
        )
        if self.corrupt_crc != "none" and matches and not self.corruption_used:
            if self.corrupt_crc == "header":
                encoded[24] ^= 0x01
            elif self.corrupt_crc == "frame":
                encoded[-1] ^= 0x01
            else:
                raise ValueError(f"unsupported corruption mode {self.corrupt_crc}")
            self.corruption_used = True
            self.log(
                f"TX {frame.type_name}: intentionally corrupted {self.corrupt_crc} CRC"
            )
        return bytes(encoded)

    def send(self, descriptor: int, frame: Frame) -> None:
        encoded = self.encoded(frame)
        self.log(f"TX {frame.type_name} req={frame.request_id} seq={frame.sequence}")
        for chunk in fragment_bytes(encoded, self.fragment_sizes):
            view = memoryview(chunk)
            while view:
                try:
                    written = os.write(descriptor, view)
                    if written <= 0:
                        raise OSError("PTY write made no progress")
                    view = view[written:]
                except BlockingIOError:
                    select.select([], [descriptor], [], 0.5)
                except InterruptedError:
                    continue
            if self.fragment_delay_seconds:
                time.sleep(self.fragment_delay_seconds)


class PtySimulator:
    def __init__(
        self,
        endpoint: PseudoTerminal,
        engine: MacRoleEngine | FlipperRoleEngine,
        transmitter: FrameTransmitter,
        *,
        log: LogFunction = _no_log,
    ):
        self.endpoint = endpoint
        self.engine = engine
        self.transmitter = transmitter
        self.log = log
        self.decoder = StreamDecoder(
            lambda issue: self.log(f"RX parse error: {issue.code.value}")
        )

    def run(
        self,
        *,
        stop_event: Event | None = None,
        timeout_seconds: float = 0,
        once: bool = False,
    ) -> None:
        stop_event = stop_event or Event()
        if isinstance(self.engine, FlipperRoleEngine):
            for frame in self.engine.initial_frames():
                self.transmitter.send(self.endpoint.master_fd, frame)

        started = time.monotonic()
        completed_at: float | None = None
        while not stop_event.is_set():
            now = time.monotonic()
            if timeout_seconds and now - started >= timeout_seconds:
                return
            if once and self.engine.complete:
                if completed_at is None:
                    completed_at = now
                elif now - completed_at >= 0.1:
                    return

            readable, _, _ = select.select([self.endpoint.master_fd], [], [], 0.05)
            if not readable:
                continue
            try:
                data = os.read(self.endpoint.master_fd, 4096)
            except BlockingIOError:
                continue
            except OSError as error:
                if error.errno in (errno.EIO, errno.ENXIO):
                    continue
                raise
            if not data:
                continue
            for frame in self.decoder.feed(data):
                for response in self.engine.on_frame(frame):
                    self.transmitter.send(self.endpoint.master_fd, response)


def _parse_fragment_sizes(value: str) -> tuple[int, ...]:
    try:
        result = tuple(int(item.strip()) for item in value.split(",") if item.strip())
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "fragment sizes must be comma-separated integers"
        ) from error
    if not result or any(size <= 0 for size in result):
        raise argparse.ArgumentTypeError("fragment sizes must be positive")
    return result


def _message_type(value: str) -> MessageType:
    try:
        return MessageType[value.upper()]
    except KeyError as error:
        names = ", ".join(item.name for item in MessageType)
        raise argparse.ArgumentTypeError(
            f"unknown message type; choose one of: {names}"
        ) from error


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--role", choices=("mac", "flipper"), required=True)
    parser.add_argument(
        "--fragment-sizes",
        type=_parse_fragment_sizes,
        default=(MAX_FRAME_SIZE,),
        metavar="N[,N...]",
        help="repeat this write-fragment pattern (default: whole frame)",
    )
    parser.add_argument(
        "--fragment-delay-ms",
        type=float,
        default=0,
        help="optional delay between write fragments",
    )
    parser.add_argument(
        "--corrupt-crc",
        choices=("none", "header", "frame"),
        default="none",
        help="corrupt one matching outbound frame",
    )
    parser.add_argument(
        "--corrupt-message",
        type=_message_type,
        help="message type to corrupt (default: first outbound frame)",
    )
    parser.add_argument(
        "--once", action="store_true", help="exit after the demo exchange"
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=0,
        help="exit after this many seconds; zero means no timeout",
    )
    parser.add_argument(
        "--request-url",
        default="https://example.com/fibp-demo",
        help="URL placed in the simulated Flipper request",
    )
    parser.add_argument(
        "--response-text",
        default="FIBP simulator HTTPS response\n",
        help="body returned by the simulated Mac role",
    )
    parser.add_argument(
        "--no-example-request",
        action="store_true",
        help="Flipper role stops after HELLO and PING",
    )
    parser.add_argument(
        "--quiet", action="store_true", help="suppress stderr diagnostics"
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_argument_parser().parse_args(argv)
    if args.fragment_delay_ms < 0 or args.fragment_delay_ms > 1000:
        raise SystemExit("--fragment-delay-ms must be between 0 and 1000")
    if args.timeout < 0:
        raise SystemExit("--timeout cannot be negative")

    log: LogFunction = (
        _no_log if args.quiet else lambda message: print(message, file=sys.stderr)
    )
    if args.role == "mac":
        engine: MacRoleEngine | FlipperRoleEngine = MacRoleEngine(
            response_text=args.response_text,
            server_nonce=secrets.randbits(64),
            log=log,
        )
    else:
        engine = FlipperRoleEngine(
            request_url=args.request_url,
            client_nonce=secrets.randbits(64),
            request_id=(secrets.randbits(32) or 1),
            ping_token=secrets.randbits(64),
            auto_request=not args.no_example_request,
            log=log,
        )

    stop_event = Event()

    def stop(_signum: int, _frame: object) -> None:
        stop_event.set()

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    with PseudoTerminal.open() as endpoint:
        # This is deliberately the only stdout record.
        print(endpoint.slave_path, flush=True)
        log(f"FIBP {args.role} simulator ready on {endpoint.slave_path}")
        transmitter = FrameTransmitter(
            args.fragment_sizes,
            fragment_delay_seconds=args.fragment_delay_ms / 1000.0,
            corrupt_crc=args.corrupt_crc,
            corrupt_message=args.corrupt_message,
            log=log,
        )
        simulator = PtySimulator(endpoint, engine, transmitter, log=log)
        simulator.run(
            stop_event=stop_event,
            timeout_seconds=args.timeout,
            once=args.once,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
