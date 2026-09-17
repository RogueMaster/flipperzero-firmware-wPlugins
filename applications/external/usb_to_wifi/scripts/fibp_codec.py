#!/usr/bin/env python3
"""FIBP v1 wire codec and bounded streaming parser.

This module intentionally uses only the Python standard library.  It mirrors the
normative C codec in ``bridge_protocol.c`` and is also used by the PTY simulator.
"""

from __future__ import annotations

import binascii
import struct
from dataclasses import dataclass
from enum import Enum, IntEnum, IntFlag
from typing import Callable, Iterable, Iterator, Sequence


MAGIC = b"FIBP"
PROTOCOL_MAJOR = 1
PROTOCOL_MINOR = 0
HEADER_SIZE = 28
HEADER_CRC_OFFSET = 24
TRAILER_SIZE = 4
MAX_FRAME_PAYLOAD = 512
MIN_NEGOTIATED_PAYLOAD = 28
MAX_FRAME_SIZE = HEADER_SIZE + MAX_FRAME_PAYLOAD + TRAILER_SIZE

MAX_URL_BYTES = 384
MAX_HEADER_NAME_BYTES = 64
MAX_HEADER_VALUE_BYTES = 256
MAX_DEVICE_MODEL_BYTES = 16
MAX_DEVICE_NAME_BYTES = 32
MAX_DEVICE_ID_BYTES = 16
MAX_APP_VERSION_BYTES = 16
MAX_ERROR_DETAIL_BYTES = 128
MAX_REQUEST_BODY_BYTES = 4096
MAX_RESPONSE_BODY_BYTES = 4 * 1024 * 1024
MAX_HEADER_COUNT = 8

_HEADER_PREFIX = struct.Struct("<4sBBBBHHIII")
_U32 = struct.Struct("<I")


class MessageType(IntEnum):
    HELLO = 0x01
    HELLO_ACK = 0x02
    PERMISSION_STATUS = 0x03
    PERMISSION_REQUIRED = 0x04
    REQUEST_START = 0x10
    REQUEST_HEADER = 0x11
    REQUEST_BODY_CHUNK = 0x12
    REQUEST_END = 0x13
    RESPONSE_START = 0x20
    RESPONSE_HEADER = 0x21
    RESPONSE_BODY_CHUNK = 0x22
    RESPONSE_END = 0x23
    CANCEL = 0x30
    PING = 0x31
    PONG = 0x32
    ERROR = 0x7E
    DISCONNECT = 0x7F


class FrameFlags(IntFlag):
    NONE = 0
    ACK_REQUIRED = 0x0001
    FINAL = 0x0002
    TRUNCATED = 0x0004
    RETRYABLE = 0x0008


class Capability(IntFlag):
    HTTPS_GET = 0x00000001
    HTTPS_POST = 0x00000002
    REQUEST_HEADERS = 0x00000004
    RESPONSE_HEADERS = 0x00000008
    CANCELLATION = 0x00000010


class ErrorCode(IntEnum):
    MALFORMED_FRAME = 0x0001
    BAD_HEADER_CRC = 0x0002
    BAD_FRAME_CRC = 0x0003
    PAYLOAD_TOO_LARGE = 0x0004
    UNSUPPORTED_VERSION = 0x0005
    UNSUPPORTED_MESSAGE = 0x0006
    DUPLICATE_SEQUENCE = 0x0007
    SEQUENCE_GAP = 0x0008
    INVALID_STATE = 0x0009
    DUPLICATE_REQUEST_ID = 0x000A
    PERMISSION_DENIED = 0x000B
    INVALID_REQUEST = 0x000C
    SECURITY_BLOCKED = 0x000D
    TIMEOUT = 0x000E
    CANCELLED = 0x000F
    RESPONSE_TOO_LARGE = 0x0010
    RX_OVERFLOW = 0x0011
    INTERNAL_ERROR = 0x0012
    TRANSPORT_LOST = 0x0013
    NETWORK_FAILURE = 0x0014


class ParseIssueCode(str, Enum):
    INVALID_HEADER = "INVALID_HEADER"
    BAD_HEADER_CRC = "BAD_HEADER_CRC"
    PAYLOAD_TOO_LARGE = "PAYLOAD_TOO_LARGE"
    BAD_FRAME_CRC = "BAD_FRAME_CRC"
    TRUNCATED_FRAME = "TRUNCATED_FRAME"


@dataclass(frozen=True)
class ParseIssue:
    code: ParseIssueCode
    detail: str


class FrameDecodeError(ValueError):
    """A bounded frame decoding failure suitable for parser diagnostics."""

    def __init__(self, code: ParseIssueCode, detail: str):
        super().__init__(detail)
        self.code = code
        self.detail = detail


@dataclass(frozen=True)
class Frame:
    message_type: int
    payload: bytes = b""
    request_id: int = 0
    sequence: int = 0
    flags: int = 0
    major: int = PROTOCOL_MAJOR
    minor: int = PROTOCOL_MINOR

    def __post_init__(self) -> None:
        object.__setattr__(self, "payload", bytes(self.payload))
        _require_uint("message_type", int(self.message_type), 8)
        _require_uint("flags", int(self.flags), 16)
        _require_uint("request_id", self.request_id, 32)
        _require_uint("sequence", self.sequence, 32)
        _require_uint("major", self.major, 8)
        _require_uint("minor", self.minor, 8)
        if len(self.payload) > MAX_FRAME_PAYLOAD:
            raise ValueError(f"payload exceeds {MAX_FRAME_PAYLOAD} bytes")

    @property
    def type_name(self) -> str:
        try:
            return MessageType(int(self.message_type)).name
        except ValueError:
            return f"UNKNOWN_0x{int(self.message_type):02X}"


def _require_uint(name: str, value: int, bits: int) -> None:
    if not isinstance(value, int) or not 0 <= value < (1 << bits):
        raise ValueError(f"{name} must be an unsigned {bits}-bit integer")


def crc32(data: bytes | bytearray | memoryview) -> int:
    """Return CRC-32/ISO-HDLC, matching the normative C implementation."""

    return binascii.crc32(data) & 0xFFFFFFFF


def encode_frame(frame: Frame) -> bytes:
    """Encode one complete FIBP frame."""

    payload = frame.payload
    prefix = _HEADER_PREFIX.pack(
        MAGIC,
        frame.major,
        frame.minor,
        HEADER_SIZE,
        int(frame.message_type),
        int(frame.flags),
        0,
        frame.request_id,
        frame.sequence,
        len(payload),
    )
    header_crc = _U32.pack(crc32(prefix))
    # The header CRC field itself is deliberately excluded from the frame CRC.
    trailer = _U32.pack(crc32(prefix + payload))
    return prefix + header_crc + payload + trailer


@dataclass(frozen=True)
class _ValidatedHeader:
    major: int
    minor: int
    message_type: int
    flags: int
    request_id: int
    sequence: int
    payload_length: int


def _decode_header(header: bytes | bytearray | memoryview) -> _ValidatedHeader:
    if len(header) < HEADER_SIZE:
        raise FrameDecodeError(ParseIssueCode.TRUNCATED_FRAME, "header is incomplete")

    (
        magic,
        major,
        minor,
        header_length,
        message_type,
        flags,
        reserved,
        request_id,
        sequence,
        payload_length,
    ) = _HEADER_PREFIX.unpack_from(header)

    if magic != MAGIC:
        raise FrameDecodeError(ParseIssueCode.INVALID_HEADER, "magic is not FIBP")
    if header_length != HEADER_SIZE:
        raise FrameDecodeError(
            ParseIssueCode.INVALID_HEADER,
            f"header length {header_length} is not {HEADER_SIZE}",
        )
    if reserved != 0:
        raise FrameDecodeError(
            ParseIssueCode.INVALID_HEADER, "reserved field is non-zero"
        )

    expected_header_crc = _U32.unpack_from(header, HEADER_CRC_OFFSET)[0]
    actual_header_crc = crc32(memoryview(header)[:HEADER_CRC_OFFSET])
    if expected_header_crc != actual_header_crc:
        raise FrameDecodeError(
            ParseIssueCode.BAD_HEADER_CRC,
            f"header CRC 0x{expected_header_crc:08x} != 0x{actual_header_crc:08x}",
        )
    if payload_length > MAX_FRAME_PAYLOAD:
        raise FrameDecodeError(
            ParseIssueCode.PAYLOAD_TOO_LARGE,
            f"payload length {payload_length} exceeds {MAX_FRAME_PAYLOAD}",
        )

    return _ValidatedHeader(
        major=major,
        minor=minor,
        message_type=message_type,
        flags=flags,
        request_id=request_id,
        sequence=sequence,
        payload_length=payload_length,
    )


def decode_frame(encoded: bytes | bytearray | memoryview) -> Frame:
    """Decode exactly one FIBP frame and reject trailing or missing bytes."""

    raw = memoryview(encoded)
    header = _decode_header(raw)
    expected_size = HEADER_SIZE + header.payload_length + TRAILER_SIZE
    if len(raw) != expected_size:
        raise FrameDecodeError(
            ParseIssueCode.TRUNCATED_FRAME,
            f"encoded length {len(raw)} does not equal declared length {expected_size}",
        )

    payload = bytes(raw[HEADER_SIZE : HEADER_SIZE + header.payload_length])
    expected_frame_crc = _U32.unpack_from(raw, HEADER_SIZE + header.payload_length)[0]
    actual_frame_crc = crc32(bytes(raw[:HEADER_CRC_OFFSET]) + payload)
    if expected_frame_crc != actual_frame_crc:
        raise FrameDecodeError(
            ParseIssueCode.BAD_FRAME_CRC,
            f"frame CRC 0x{expected_frame_crc:08x} != 0x{actual_frame_crc:08x}",
        )

    return Frame(
        major=header.major,
        minor=header.minor,
        message_type=header.message_type,
        flags=header.flags,
        request_id=header.request_id,
        sequence=header.sequence,
        payload=payload,
    )


class StreamDecoder:
    """Incremental byte-stream decoder with bounded magic resynchronisation."""

    def __init__(self, on_issue: Callable[[ParseIssue], None] | None = None):
        self._buffer = bytearray()
        self._on_issue = on_issue
        self.issues: list[ParseIssue] = []

    @property
    def buffered_bytes(self) -> int:
        return len(self._buffer)

    def reset(self) -> None:
        self._buffer.clear()

    def _report(self, error: FrameDecodeError) -> None:
        issue = ParseIssue(error.code, error.detail)
        self.issues.append(issue)
        if self._on_issue is not None:
            self._on_issue(issue)

    def _discard_until_magic(self) -> bool:
        location = self._buffer.find(MAGIC)
        if location >= 0:
            if location:
                del self._buffer[:location]
            return True

        # Preserve only a suffix that can still become the magic prefix after
        # the next read.  Random serial/CLI bytes therefore cannot grow memory.
        keep = 0
        maximum = min(len(self._buffer), len(MAGIC) - 1)
        for length in range(1, maximum + 1):
            if self._buffer[-length:] == MAGIC[:length]:
                keep = length
        if keep:
            del self._buffer[:-keep]
        else:
            self._buffer.clear()
        return False

    def feed(self, data: bytes | bytearray | memoryview) -> list[Frame]:
        self._buffer.extend(data)
        frames: list[Frame] = []

        while True:
            if not self._discard_until_magic():
                break
            if len(self._buffer) < HEADER_SIZE:
                break

            try:
                # Pass an immutable copy: a live memoryview would prevent the
                # bytearray from being resized during error resynchronisation.
                header = _decode_header(bytes(self._buffer[:HEADER_SIZE]))
            except FrameDecodeError as error:
                self._report(error)
                # Drop one byte, not a claimed frame length.  The remaining
                # bytes are scanned again so a following valid frame survives.
                del self._buffer[0]
                continue

            encoded_size = HEADER_SIZE + header.payload_length + TRAILER_SIZE
            if len(self._buffer) < encoded_size:
                break

            candidate = bytes(self._buffer[:encoded_size])
            try:
                frame = decode_frame(candidate)
            except FrameDecodeError as error:
                self._report(error)
                del self._buffer[0]
                continue

            frames.append(frame)
            del self._buffer[:encoded_size]

        return frames

    def finish(self) -> list[ParseIssue]:
        """Report a meaningful partial frame when the transport reaches EOF."""

        if self._buffer:
            issue = ParseIssue(
                ParseIssueCode.TRUNCATED_FRAME, "transport ended mid-frame"
            )
            self.issues.append(issue)
            if self._on_issue is not None:
                self._on_issue(issue)
            self._buffer.clear()
            return [issue]
        return []


class PayloadDecodeError(ValueError):
    pass


class _Reader:
    def __init__(self, payload: bytes):
        self.payload = payload
        self.offset = 0

    def take(self, length: int, field: str) -> bytes:
        if length < 0 or self.offset + length > len(self.payload):
            raise PayloadDecodeError(f"{field} exceeds payload")
        value = self.payload[self.offset : self.offset + length]
        self.offset += length
        return value

    def u8(self, field: str) -> int:
        return self.take(1, field)[0]

    def u16(self, field: str) -> int:
        return struct.unpack("<H", self.take(2, field))[0]

    def u32(self, field: str) -> int:
        return struct.unpack("<I", self.take(4, field))[0]

    def u64(self, field: str) -> int:
        return struct.unpack("<Q", self.take(8, field))[0]

    def done(self) -> None:
        if self.offset != len(self.payload):
            raise PayloadDecodeError(
                f"{len(self.payload) - self.offset} trailing payload bytes"
            )


def _encoded_text(
    value: str,
    field: str,
    maximum: int,
    *,
    allow_empty: bool = False,
    allow_tab: bool = False,
) -> bytes:
    encoded = value.encode("utf-8", errors="strict")
    if (not allow_empty and not encoded) or len(encoded) > maximum:
        minimum = 0 if allow_empty else 1
        raise ValueError(f"{field} UTF-8 length must be {minimum}...{maximum}")
    for byte in encoded:
        if (
            byte == 0
            or byte == 0x7F
            or (byte < 0x20 and not (allow_tab and byte == 0x09))
        ):
            raise ValueError(f"{field} contains a disallowed control character")
    return encoded


def _decoded_text(
    value: bytes,
    field: str,
    maximum: int,
    *,
    allow_empty: bool = False,
    allow_tab: bool = False,
) -> str:
    try:
        decoded = value.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        raise PayloadDecodeError(f"{field} is not valid UTF-8") from error
    try:
        _encoded_text(
            decoded,
            field,
            maximum,
            allow_empty=allow_empty,
            allow_tab=allow_tab,
        )
    except ValueError as error:
        raise PayloadDecodeError(str(error)) from error
    return decoded


@dataclass(frozen=True)
class Hello:
    minimum_major: int
    minimum_minor: int
    maximum_major: int
    maximum_minor: int
    capabilities: int
    maximum_rx_payload: int
    maximum_response_bytes: int
    client_nonce: int
    model: str
    name: str
    id_type: int
    device_id: bytes
    app_version: str


def encode_hello(value: Hello) -> bytes:
    for name in ("minimum_major", "minimum_minor", "maximum_major", "maximum_minor"):
        _require_uint(name, getattr(value, name), 8)
    _require_uint("capabilities", int(value.capabilities), 32)
    if not MIN_NEGOTIATED_PAYLOAD <= value.maximum_rx_payload <= MAX_FRAME_PAYLOAD:
        raise ValueError("maximum_rx_payload is outside v1 bounds")
    if not 1 <= value.maximum_response_bytes <= MAX_RESPONSE_BODY_BYTES:
        raise ValueError("maximum_response_bytes is outside v1 bounds")
    _require_uint("client_nonce", value.client_nonce, 64)
    _require_uint("id_type", value.id_type, 8)

    model = _encoded_text(value.model, "model", MAX_DEVICE_MODEL_BYTES)
    name = _encoded_text(value.name, "name", MAX_DEVICE_NAME_BYTES)
    app_version = _encoded_text(value.app_version, "app_version", MAX_APP_VERSION_BYTES)
    device_id = bytes(value.device_id)
    if not 1 <= len(device_id) <= MAX_DEVICE_ID_BYTES:
        raise ValueError("device_id length is outside v1 bounds")

    fixed = struct.pack(
        "<BBBBIHIQ",
        value.minimum_major,
        value.minimum_minor,
        value.maximum_major,
        value.maximum_minor,
        int(value.capabilities),
        value.maximum_rx_payload,
        value.maximum_response_bytes,
        value.client_nonce,
    )
    return (
        fixed
        + bytes((len(model),))
        + model
        + bytes((len(name),))
        + name
        + bytes((value.id_type, len(device_id)))
        + device_id
        + bytes((len(app_version),))
        + app_version
    )


def decode_hello(payload: bytes) -> Hello:
    reader = _Reader(payload)
    minimum_major = reader.u8("minimum_major")
    minimum_minor = reader.u8("minimum_minor")
    maximum_major = reader.u8("maximum_major")
    maximum_minor = reader.u8("maximum_minor")
    capabilities = reader.u32("capabilities")
    maximum_rx_payload = reader.u16("maximum_rx_payload")
    maximum_response_bytes = reader.u32("maximum_response_bytes")
    client_nonce = reader.u64("client_nonce")
    model = _decoded_text(
        reader.take(reader.u8("model_length"), "model"), "model", MAX_DEVICE_MODEL_BYTES
    )
    name = _decoded_text(
        reader.take(reader.u8("name_length"), "name"), "name", MAX_DEVICE_NAME_BYTES
    )
    id_type = reader.u8("id_type")
    device_id = reader.take(reader.u8("device_id_length"), "device_id")
    if not 1 <= len(device_id) <= MAX_DEVICE_ID_BYTES:
        raise PayloadDecodeError("device_id length is outside v1 bounds")
    app_version = _decoded_text(
        reader.take(reader.u8("app_version_length"), "app_version"),
        "app_version",
        MAX_APP_VERSION_BYTES,
    )
    reader.done()
    if not MIN_NEGOTIATED_PAYLOAD <= maximum_rx_payload <= MAX_FRAME_PAYLOAD:
        raise PayloadDecodeError("maximum_rx_payload is outside v1 bounds")
    if not 1 <= maximum_response_bytes <= MAX_RESPONSE_BODY_BYTES:
        raise PayloadDecodeError("maximum_response_bytes is outside v1 bounds")
    return Hello(
        minimum_major,
        minimum_minor,
        maximum_major,
        maximum_minor,
        capabilities,
        maximum_rx_payload,
        maximum_response_bytes,
        client_nonce,
        model,
        name,
        id_type,
        device_id,
        app_version,
    )


@dataclass(frozen=True)
class HelloAck:
    selected_major: int
    selected_minor: int
    capabilities: int
    maximum_payload: int
    maximum_response_bytes: int
    echoed_client_nonce: int
    server_nonce: int


def encode_hello_ack(value: HelloAck) -> bytes:
    _require_uint("selected_major", value.selected_major, 8)
    _require_uint("selected_minor", value.selected_minor, 8)
    _require_uint("capabilities", int(value.capabilities), 32)
    if not MIN_NEGOTIATED_PAYLOAD <= value.maximum_payload <= MAX_FRAME_PAYLOAD:
        raise ValueError("maximum_payload is outside v1 bounds")
    if not 1 <= value.maximum_response_bytes <= MAX_RESPONSE_BODY_BYTES:
        raise ValueError("maximum_response_bytes is outside v1 bounds")
    _require_uint("echoed_client_nonce", value.echoed_client_nonce, 64)
    _require_uint("server_nonce", value.server_nonce, 64)
    return struct.pack(
        "<BBIHIQQ",
        value.selected_major,
        value.selected_minor,
        int(value.capabilities),
        value.maximum_payload,
        value.maximum_response_bytes,
        value.echoed_client_nonce,
        value.server_nonce,
    )


def decode_hello_ack(payload: bytes) -> HelloAck:
    if len(payload) != struct.calcsize("<BBIHIQQ"):
        raise PayloadDecodeError("HELLO_ACK has the wrong length")
    value = HelloAck(*struct.unpack("<BBIHIQQ", payload))
    if not MIN_NEGOTIATED_PAYLOAD <= value.maximum_payload <= MAX_FRAME_PAYLOAD:
        raise PayloadDecodeError("maximum_payload is outside v1 bounds")
    if not 1 <= value.maximum_response_bytes <= MAX_RESPONSE_BODY_BYTES:
        raise PayloadDecodeError("maximum_response_bytes is outside v1 bounds")
    return value


@dataclass(frozen=True)
class RequestStart:
    method: int
    timeout_ms: int
    url: str
    declared_body_length: int = 0
    declared_header_count: int = 0


def encode_request_start(value: RequestStart) -> bytes:
    if value.method not in (1, 2):
        raise ValueError("method must be GET(1) or POST(2)")
    if not 1 <= value.timeout_ms <= 30000:
        raise ValueError("timeout_ms must be 1...30000")
    if not 0 <= value.declared_body_length <= MAX_REQUEST_BODY_BYTES:
        raise ValueError("declared_body_length exceeds v1 limit")
    if value.method == 1 and value.declared_body_length != 0:
        raise ValueError("GET cannot declare a body")
    if not 0 <= value.declared_header_count <= MAX_HEADER_COUNT:
        raise ValueError("declared_header_count exceeds v1 limit")
    url = _encoded_text(value.url, "url", MAX_URL_BYTES)
    return (
        struct.pack(
            "<BIHIB",
            value.method,
            value.timeout_ms,
            len(url),
            value.declared_body_length,
            value.declared_header_count,
        )
        + url
    )


def decode_request_start(payload: bytes) -> RequestStart:
    reader = _Reader(payload)
    method = reader.u8("method")
    timeout_ms = reader.u32("timeout_ms")
    url_length = reader.u16("url_length")
    body_length = reader.u32("declared_body_length")
    header_count = reader.u8("declared_header_count")
    url = _decoded_text(reader.take(url_length, "url"), "url", MAX_URL_BYTES)
    reader.done()
    value = RequestStart(method, timeout_ms, url, body_length, header_count)
    try:
        encode_request_start(value)
    except ValueError as error:
        raise PayloadDecodeError(str(error)) from error
    return value


@dataclass(frozen=True)
class HeaderField:
    name: str
    value: str


_HTTP_TOKEN = frozenset(
    "!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
)


def encode_header(value: HeaderField) -> bytes:
    name = _encoded_text(value.name, "header name", MAX_HEADER_NAME_BYTES)
    if any(chr(byte) not in _HTTP_TOKEN for byte in name):
        raise ValueError("header name is not an RFC token")
    encoded_value = _encoded_text(
        value.value,
        "header value",
        MAX_HEADER_VALUE_BYTES,
        allow_empty=True,
        allow_tab=True,
    )
    if b"\r" in encoded_value or b"\n" in encoded_value:
        raise ValueError("header value contains CR or LF")
    return struct.pack("<BH", len(name), len(encoded_value)) + name + encoded_value


def decode_header(payload: bytes) -> HeaderField:
    reader = _Reader(payload)
    name_length = reader.u8("name_length")
    value_length = reader.u16("value_length")
    name = _decoded_text(
        reader.take(name_length, "header name"), "header name", MAX_HEADER_NAME_BYTES
    )
    value = _decoded_text(
        reader.take(value_length, "header value"),
        "header value",
        MAX_HEADER_VALUE_BYTES,
        allow_empty=True,
        allow_tab=True,
    )
    reader.done()
    result = HeaderField(name, value)
    try:
        encode_header(result)
    except ValueError as error:
        raise PayloadDecodeError(str(error)) from error
    return result


@dataclass(frozen=True)
class ResponseStart:
    http_status: int
    declared_header_count: int
    declared_body_length: int


def encode_response_start(value: ResponseStart) -> bytes:
    if not 100 <= value.http_status <= 599:
        raise ValueError("http_status must be 100...599")
    if not 0 <= value.declared_header_count <= MAX_HEADER_COUNT:
        raise ValueError("declared_header_count exceeds v1 limit")
    if value.declared_body_length != 0xFFFFFFFF and not (
        0 <= value.declared_body_length <= MAX_RESPONSE_BODY_BYTES
    ):
        raise ValueError("declared_body_length exceeds v1 limit")
    return struct.pack(
        "<HBBI",
        value.http_status,
        value.declared_header_count,
        0,
        value.declared_body_length,
    )


def decode_response_start(payload: bytes) -> ResponseStart:
    if len(payload) != struct.calcsize("<HBBI"):
        raise PayloadDecodeError("RESPONSE_START has the wrong length")
    status, header_count, reserved, body_length = struct.unpack("<HBBI", payload)
    if reserved != 0:
        raise PayloadDecodeError("RESPONSE_START reserved byte is non-zero")
    value = ResponseStart(status, header_count, body_length)
    try:
        encode_response_start(value)
    except ValueError as error:
        raise PayloadDecodeError(str(error)) from error
    return value


@dataclass(frozen=True)
class ResponseEnd:
    result: int
    bytes_sent: int


def encode_response_end(value: ResponseEnd) -> bytes:
    if value.result not in (0, 1, 2):
        raise ValueError("response result must be complete, truncated, or cancelled")
    if not 0 <= value.bytes_sent <= MAX_RESPONSE_BODY_BYTES:
        raise ValueError("bytes_sent exceeds v1 limit")
    return struct.pack("<BI", value.result, value.bytes_sent)


def decode_response_end(payload: bytes) -> ResponseEnd:
    if len(payload) != struct.calcsize("<BI"):
        raise PayloadDecodeError("RESPONSE_END has the wrong length")
    value = ResponseEnd(*struct.unpack("<BI", payload))
    try:
        encode_response_end(value)
    except ValueError as error:
        raise PayloadDecodeError(str(error)) from error
    return value


def encode_permission_status(state: int, reason: int = 0) -> bytes:
    if state not in (0, 1, 2) or reason not in (0, 1, 2):
        raise ValueError("invalid permission status")
    return bytes((state, reason))


def decode_permission_status(payload: bytes) -> tuple[int, int]:
    if len(payload) != 2 or payload[0] not in (0, 1, 2) or payload[1] not in (0, 1, 2):
        raise PayloadDecodeError("invalid PERMISSION_STATUS")
    return payload[0], payload[1]


def encode_ping_token(token: int) -> bytes:
    _require_uint("token", token, 64)
    return struct.pack("<Q", token)


def decode_ping_token(payload: bytes) -> int:
    if len(payload) != 8:
        raise PayloadDecodeError("PING/PONG token must be exactly 8 bytes")
    return struct.unpack("<Q", payload)[0]


def fragment_bytes(data: bytes, sizes: Sequence[int]) -> Iterator[bytes]:
    """Yield data using a repeating positive fragment-size pattern."""

    if not sizes or any(size <= 0 for size in sizes):
        raise ValueError("fragment sizes must be a non-empty list of positive integers")
    offset = 0
    index = 0
    while offset < len(data):
        size = sizes[index % len(sizes)]
        yield data[offset : offset + size]
        offset += size
        index += 1


def frames_from_chunks(chunks: Iterable[bytes]) -> list[Frame]:
    """Convenience helper used by tests and small diagnostic scripts."""

    decoder = StreamDecoder()
    frames: list[Frame] = []
    for chunk in chunks:
        frames.extend(decoder.feed(chunk))
    return frames


__all__ = [name for name in globals() if not name.startswith("_")]
