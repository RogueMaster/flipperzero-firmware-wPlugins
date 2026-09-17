from __future__ import annotations

import secrets
import struct
from collections import deque
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import IntEnum
from threading import Event, Lock

from scripts.fibp_codec import (
    MAX_REQUEST_BODY_BYTES,
    MAX_RESPONSE_BODY_BYTES,
    Capability,
    ErrorCode,
    Frame,
    FrameFlags,
    HeaderField,
    Hello,
    HelloAck,
    MessageType,
    PayloadDecodeError,
    RequestStart,
    ResponseEnd,
    ResponseStart,
    decode_header,
    decode_hello,
    decode_ping_token,
    decode_request_start,
    encode_header,
    encode_hello_ack,
    encode_permission_status,
    encode_ping_token,
    encode_response_end,
    encode_response_start,
)


class PermissionDecision(IntEnum):
    DENY = 0
    ALLOW_ONCE = 1
    ALLOW_ALWAYS = 2


@dataclass
class PendingRequest:
    request_id: int
    start: RequestStart
    expected_sequence: int = 1
    headers: list[HeaderField] = field(default_factory=list)
    body: bytearray = field(default_factory=bytearray)
    header_bytes: int = 0


@dataclass(frozen=True)
class CompletedRequest:
    request_id: int
    start: RequestStart
    headers: tuple[HeaderField, ...]
    body: bytes
    maximum_response_bytes: int
    next_client_sequence: int
    cancel: Event


RequestCallback = Callable[[CompletedRequest], None]


def _error_payload(code: ErrorCode, scope: int, offending: int, detail: str) -> bytes:
    encoded = detail.encode("utf-8", errors="replace")[:128]
    return (
        struct.pack("<HBBH", int(code), scope, offending & 0xFF, len(encoded)) + encoded
    )


class HostSession:
    CAPABILITIES = int(
        Capability.HTTPS_GET
        | Capability.HTTPS_POST
        | Capability.REQUEST_HEADERS
        | Capability.RESPONSE_HEADERS
        | Capability.CANCELLATION
    )

    def __init__(
        self,
        on_request: RequestCallback,
        persistent_permission: Callable[[Hello], bool],
    ):
        self.on_request = on_request
        self.persistent_permission = persistent_permission
        self.hello: Hello | None = None
        self.permission = PermissionDecision.DENY
        self.permission_pending = False
        self.control_sequence = 0
        self.expected_client_control_sequence = 1
        self.pending: PendingRequest | None = None
        self.active: CompletedRequest | None = None
        self._seen_ids: deque[int] = deque(maxlen=64)
        self._lock = Lock()

    def _control_frame(
        self, message_type: MessageType, payload: bytes = b"", request_id: int = 0
    ) -> Frame:
        with self._lock:
            sequence = self.control_sequence
            self.control_sequence += 1
        frame = Frame(message_type, payload, request_id=request_id, sequence=sequence)
        return frame

    def _error(
        self, code: ErrorCode, frame: Frame, detail: str, request_scope: bool = True
    ) -> Frame:
        return self._control_frame(
            MessageType.ERROR,
            _error_payload(code, 1 if request_scope else 0, frame.message_type, detail),
            frame.request_id if request_scope else 0,
        )

    def _reset_for_hello(self, hello: Hello) -> None:
        if self.active:
            self.active.cancel.set()
        self.hello = hello
        self.permission = PermissionDecision.DENY
        self.permission_pending = False
        self.control_sequence = 0
        self.expected_client_control_sequence = 1
        self.pending = None
        self.active = None
        self._seen_ids.clear()

    def on_frame(self, frame: Frame) -> list[Frame]:
        if frame.major != 1 or frame.minor != 0:
            return [
                self._error(
                    ErrorCode.UNSUPPORTED_VERSION,
                    frame,
                    "unsupported frame version",
                    False,
                )
            ]
        try:
            message_type = MessageType(frame.message_type)
        except ValueError:
            return [
                self._error(
                    ErrorCode.UNSUPPORTED_MESSAGE, frame, "unknown message type"
                )
            ]
        try:
            if message_type == MessageType.HELLO:
                if frame.request_id != 0 or frame.sequence != 0:
                    return [
                        self._error(
                            ErrorCode.INVALID_STATE, frame, "invalid HELLO", False
                        )
                    ]
                hello = decode_hello(frame.payload)
                if not (hello.minimum_major <= 1 <= hello.maximum_major):
                    return [
                        self._error(
                            ErrorCode.UNSUPPORTED_VERSION,
                            frame,
                            "FIBP v1 required",
                            False,
                        )
                    ]
                if not hello.model.lower().startswith("flipper") or hello.id_type != 1:
                    return [
                        self._error(
                            ErrorCode.INVALID_REQUEST,
                            frame,
                            "device identity is not a Flipper",
                            False,
                        )
                    ]
                self._reset_for_hello(hello)
                limit = min(hello.maximum_response_bytes, MAX_RESPONSE_BODY_BYTES)
                ack = HelloAck(
                    1,
                    0,
                    hello.capabilities & self.CAPABILITIES,
                    min(hello.maximum_rx_payload, 512),
                    limit,
                    hello.client_nonce,
                    secrets.randbits(64),
                )
                responses = [
                    self._control_frame(MessageType.HELLO_ACK, encode_hello_ack(ack))
                ]
                if self.persistent_permission(hello):
                    self.permission = PermissionDecision.ALLOW_ALWAYS
                    responses.append(
                        self._control_frame(
                            MessageType.PERMISSION_STATUS,
                            encode_permission_status(2, 1),
                        )
                    )
                else:
                    self.permission_pending = True
                    responses.append(
                        self._control_frame(
                            MessageType.PERMISSION_REQUIRED, bytes((0,))
                        )
                    )
                return responses

            if self.hello is None:
                return [
                    self._error(ErrorCode.INVALID_STATE, frame, "HELLO required", False)
                ]
            if message_type == MessageType.PING:
                if (
                    frame.request_id != 0
                    or frame.sequence != self.expected_client_control_sequence
                ):
                    return [
                        self._error(
                            ErrorCode.SEQUENCE_GAP,
                            frame,
                            "unexpected control sequence",
                            False,
                        )
                    ]
                self.expected_client_control_sequence += 1
                return [
                    self._control_frame(
                        MessageType.PONG,
                        encode_ping_token(decode_ping_token(frame.payload)),
                    )
                ]
            if message_type == MessageType.PONG:
                if (
                    frame.request_id != 0
                    or frame.sequence != self.expected_client_control_sequence
                ):
                    return [
                        self._error(
                            ErrorCode.SEQUENCE_GAP,
                            frame,
                            "unexpected control sequence",
                            False,
                        )
                    ]
                self.expected_client_control_sequence += 1
                decode_ping_token(frame.payload)
                return []
            if message_type == MessageType.REQUEST_START:
                if self.permission not in {
                    PermissionDecision.ALLOW_ONCE,
                    PermissionDecision.ALLOW_ALWAYS,
                }:
                    return [
                        self._error(
                            ErrorCode.PERMISSION_DENIED, frame, "permission required"
                        )
                    ]
                if (
                    frame.request_id == 0
                    or frame.sequence != 0
                    or self.pending
                    or self.active
                ):
                    return [
                        self._error(
                            ErrorCode.INVALID_STATE, frame, "request cannot start"
                        )
                    ]
                if frame.request_id in self._seen_ids:
                    return [
                        self._error(
                            ErrorCode.DUPLICATE_REQUEST_ID, frame, "request id reused"
                        )
                    ]
                self.pending = PendingRequest(
                    frame.request_id, decode_request_start(frame.payload)
                )
                return []
            if message_type in {
                MessageType.REQUEST_HEADER,
                MessageType.REQUEST_BODY_CHUNK,
                MessageType.REQUEST_END,
            }:
                pending = self.pending
                if not pending or pending.request_id != frame.request_id:
                    return [
                        self._error(
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
                    return [self._error(code, frame, "unexpected request sequence")]
                pending.expected_sequence += 1
                if message_type == MessageType.REQUEST_HEADER:
                    header = decode_header(frame.payload)
                    pending.headers.append(header)
                    pending.header_bytes += len(header.name.encode()) + len(
                        header.value.encode()
                    )
                    if (
                        len(pending.headers) > pending.start.declared_header_count
                        or pending.header_bytes > 1024
                    ):
                        self.pending = None
                        return [
                            self._error(
                                ErrorCode.INVALID_REQUEST,
                                frame,
                                "request headers exceed limits",
                            )
                        ]
                    return []
                if message_type == MessageType.REQUEST_BODY_CHUNK:
                    pending.body.extend(frame.payload)
                    if len(pending.body) > min(
                        pending.start.declared_body_length, MAX_REQUEST_BODY_BYTES
                    ):
                        self.pending = None
                        return [
                            self._error(
                                ErrorCode.INVALID_REQUEST,
                                frame,
                                "request body exceeds limits",
                            )
                        ]
                    return []
                if frame.payload or not (frame.flags & FrameFlags.FINAL):
                    self.pending = None
                    return [
                        self._error(
                            ErrorCode.INVALID_REQUEST, frame, "invalid REQUEST_END"
                        )
                    ]
                if (
                    len(pending.headers) != pending.start.declared_header_count
                    or len(pending.body) != pending.start.declared_body_length
                ):
                    self.pending = None
                    return [
                        self._error(
                            ErrorCode.INVALID_REQUEST, frame, "request is incomplete"
                        )
                    ]
                completed = CompletedRequest(
                    pending.request_id,
                    pending.start,
                    tuple(pending.headers),
                    bytes(pending.body),
                    min(self.hello.maximum_response_bytes, MAX_RESPONSE_BODY_BYTES),
                    pending.expected_sequence,
                    Event(),
                )
                self.pending = None
                self.active = completed
                self._seen_ids.append(completed.request_id)
                self.on_request(completed)
                return []
            if message_type == MessageType.CANCEL:
                if len(frame.payload) != 1:
                    return [
                        self._error(ErrorCode.INVALID_REQUEST, frame, "invalid CANCEL")
                    ]
                expected = None
                if self.active and self.active.request_id == frame.request_id:
                    expected = self.active.next_client_sequence
                elif self.pending and self.pending.request_id == frame.request_id:
                    expected = self.pending.expected_sequence
                if expected is None:
                    return [
                        self._error(
                            ErrorCode.INVALID_STATE, frame, "no matching request"
                        )
                    ]
                if frame.sequence != expected:
                    code = (
                        ErrorCode.DUPLICATE_SEQUENCE
                        if frame.sequence < expected
                        else ErrorCode.SEQUENCE_GAP
                    )
                    return [self._error(code, frame, "unexpected CANCEL sequence")]
                if self.active:
                    self.active.cancel.set()
                elif self.pending:
                    self.pending = None
                return []
            if message_type in {MessageType.ERROR, MessageType.DISCONNECT}:
                self.close()
                return []
            return [
                self._error(
                    ErrorCode.INVALID_STATE, frame, "message has wrong direction"
                )
            ]
        except (PayloadDecodeError, ValueError) as error:
            self.pending = None
            return [self._error(ErrorCode.INVALID_REQUEST, frame, str(error))]

    def resolve_permission(self, decision: PermissionDecision) -> list[Frame]:
        if not self.permission_pending:
            return []
        self.permission_pending = False
        self.permission = decision
        return [
            self._control_frame(
                MessageType.PERMISSION_STATUS,
                encode_permission_status(int(decision), 0),
            )
        ]

    def finish_request(self, request_id: int) -> None:
        with self._lock:
            if self.active and self.active.request_id == request_id:
                self.active = None

    def request_error(self, request_id: int, code: ErrorCode, detail: str) -> Frame:
        synthetic = Frame(MessageType.REQUEST_END, request_id=request_id)
        return self._error(code, synthetic, detail)

    def close(self) -> None:
        if self.active:
            self.active.cancel.set()
        self.pending = None
        self.active = None


def response_start_frame(
    request_id: int,
    status: int,
    headers: list[HeaderField],
    declared_length: int,
) -> Frame:
    return Frame(
        MessageType.RESPONSE_START,
        encode_response_start(ResponseStart(status, len(headers), declared_length)),
        request_id=request_id,
        sequence=0,
    )


def response_header_frames(request_id: int, headers: list[HeaderField]) -> list[Frame]:
    return [
        Frame(
            MessageType.RESPONSE_HEADER,
            encode_header(header),
            request_id=request_id,
            sequence=index,
        )
        for index, header in enumerate(headers, start=1)
    ]


def response_end_frame(
    request_id: int,
    sequence: int,
    result: int,
    bytes_sent: int,
) -> Frame:
    flags = FrameFlags.FINAL | (
        FrameFlags.TRUNCATED if result == 1 else FrameFlags.NONE
    )
    return Frame(
        MessageType.RESPONSE_END,
        encode_response_end(ResponseEnd(result, bytes_sent)),
        request_id=request_id,
        sequence=sequence,
        flags=flags,
    )
