"""Minimal macOS PC/SC transport and DESFire probes."""

import ctypes
import ctypes.util
import os
import sys
import time
from dataclasses import dataclass, field

from cryptography.hazmat.primitives.ciphers import Cipher, modes

try:
    from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES
except ImportError:
    from cryptography.hazmat.primitives.ciphers.algorithms import TripleDES

from .credential import ReadFixture


class IORequest(ctypes.Structure):
    _fields_ = [("protocol", ctypes.c_uint32), ("length", ctypes.c_uint32)]


def native_apdu(ins: int, data: bytes = b"", le: bool = True) -> bytes:
    if not data:
        return bytes([0x90, ins, 0, 0, 0]) if le else bytes([0x90, ins, 0, 0])
    return bytes([0x90, ins, 0, 0, len(data)]) + data + (b"\0" if le else b"")


def _crypt(key: bytes, iv: bytes, data: bytes, encrypt: bool) -> bytes:
    if len(key) == 16:
        key += key[:8]
    cipher = Cipher(TripleDES(key), modes.CBC(iv))
    operation = cipher.encryptor() if encrypt else cipher.decryptor()
    return operation.update(data) + operation.finalize()


def legacy_cmac(key: bytes, iv: bytes, message: bytes) -> bytes:
    zero = bytes(8)

    def block(value: bytes) -> bytes:
        return _crypt(key, zero, value, True)

    def shift(value: bytes) -> bytes:
        number = int.from_bytes(value, "big")
        number = ((number << 1) & ((1 << 64) - 1)) ^ (0x1B if number >> 63 else 0)
        return number.to_bytes(8, "big")

    k1 = shift(block(zero))
    k2 = shift(k1)
    complete = bool(message) and len(message) % 8 == 0
    head = message[:-8] if complete else message[: len(message) // 8 * 8]
    last = (
        message[-8:]
        if complete
        else message[len(head) :] + b"\x80" + bytes(7 - len(message[len(head) :]))
    )
    mask = k1 if complete else k2
    state = iv
    for offset in range(0, len(head), 8):
        state = block(bytes(a ^ b for a, b in zip(state, head[offset : offset + 8])))
    return block(bytes(a ^ b ^ c for a, b, c in zip(state, last, mask)))


@dataclass
class PcscResult:
    timings_ms: dict[str, list[float]] = field(default_factory=dict)
    frames: int = 0
    bytes_read: int = 0

    def record(self, name: str, elapsed_ms: float) -> None:
        self.timings_ms.setdefault(name, []).append(elapsed_ms)


class PcscCard:
    def __init__(self, reader_name: str, connect_timeout: float = 30.0):
        if sys.platform != "darwin":
            raise RuntimeError("closed-loop PC/SC tests currently support macOS only")
        library = ctypes.util.find_library("PCSC")
        if not library:
            raise RuntimeError("PC/SC framework was not found")
        self.lib = ctypes.CDLL(library)
        self.handle = ctypes.c_int32
        self.dword = ctypes.c_uint32
        self._configure()
        self.reader_name = reader_name
        self.connect_timeout = connect_timeout
        self.context = self.handle()
        self.card = self.handle()
        self.protocol = self.dword()
        self.request = None

    def _configure(self) -> None:
        self.lib.SCardEstablishContext.argtypes = [
            self.dword,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.POINTER(self.handle),
        ]
        self.lib.SCardListReaders.argtypes = [
            self.handle,
            ctypes.c_char_p,
            ctypes.c_void_p,
            ctypes.POINTER(self.dword),
        ]
        self.lib.SCardConnect.argtypes = [
            self.handle,
            ctypes.c_char_p,
            self.dword,
            self.dword,
            ctypes.POINTER(self.handle),
            ctypes.POINTER(self.dword),
        ]
        self.lib.SCardTransmit.argtypes = [
            self.handle,
            ctypes.POINTER(IORequest),
            ctypes.c_void_p,
            self.dword,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.POINTER(self.dword),
        ]
        self.lib.SCardDisconnect.argtypes = [self.handle, self.dword]
        self.lib.SCardReleaseContext.argtypes = [self.handle]

    @staticmethod
    def _check(result: int) -> None:
        if result:
            raise RuntimeError(f"PCSC 0x{result & 0xFFFFFFFF:08X}")

    def __enter__(self):
        self._check(
            self.lib.SCardEstablishContext(2, None, None, ctypes.byref(self.context))
        )
        size = self.dword()
        self._check(
            self.lib.SCardListReaders(self.context, None, None, ctypes.byref(size))
        )
        names = ctypes.create_string_buffer(size.value)
        self._check(
            self.lib.SCardListReaders(self.context, None, names, ctypes.byref(size))
        )
        try:
            reader = next(
                name
                for name in names.raw.split(b"\0")
                if self.reader_name.encode() in name
            )
        except StopIteration as error:
            available = [
                name.decode(errors="replace") for name in names.raw.split(b"\0") if name
            ]
            raise RuntimeError(
                f"reader containing {self.reader_name!r} not found; available: {available}"
            ) from error
        deadline = time.monotonic() + self.connect_timeout
        while True:
            result = self.lib.SCardConnect(
                self.context,
                reader,
                2,
                3,
                ctypes.byref(self.card),
                ctypes.byref(self.protocol),
            )
            if result == 0:
                break
            if result & 0xFFFFFFFF != 0x8010000C or time.monotonic() >= deadline:
                self._check(result)
            time.sleep(0.25)
        self.request = IORequest(self.protocol.value, ctypes.sizeof(IORequest))
        return self

    def __exit__(self, *_):
        if self.card.value:
            self.lib.SCardDisconnect(self.card, 0)
        if self.context.value:
            self.lib.SCardReleaseContext(self.context)

    def exchange(self, command: bytes) -> tuple[bytes, int, float]:
        response = ctypes.create_string_buffer(4096)
        length = self.dword(len(response))
        started = time.perf_counter()
        self._check(
            self.lib.SCardTransmit(
                self.card,
                ctypes.byref(self.request),
                command,
                len(command),
                None,
                response,
                ctypes.byref(length),
            )
        )
        elapsed = (time.perf_counter() - started) * 1000
        value = response.raw[: length.value]
        if len(value) < 2 or value[-2] != 0x91:
            raise RuntimeError(f"unexpected response tail: {value[-2:].hex()}")
        return value[:-2], value[-1], elapsed


def _exchange(
    card: PcscCard, result: PcscResult, name: str, command: bytes
) -> tuple[bytes, int]:
    data, status, elapsed = card.exchange(command)
    result.record(name, elapsed)
    result.frames += 1
    return data, status


def run_iso_2k3des_read(card: PcscCard, fixture: ReadFixture) -> PcscResult:
    if len(fixture.key) != 16:
        raise ValueError("ISO 2K3DES requires a 16-byte key")
    result = PcscResult()
    version = bytearray()
    data, status = _exchange(card, result, "get_version", native_apdu(0x60))
    version.extend(data)
    while status == 0xAF:
        data, status = _exchange(
            card, result, "get_version_continuation", native_apdu(0xAF)
        )
        version.extend(data)
    if status != 0 or len(version) < 21:
        raise AssertionError(f"GetVersion status={status:02X} bytes={len(version)}")
    if bytes(version[14:21]) != fixture.uid:
        raise AssertionError(
            f"GetVersion UID {bytes(version[14:21]).hex()} != fixture UID {fixture.uid.hex()}"
        )

    _, status = _exchange(
        card, result, "select_application", native_apdu(0x5A, fixture.aid)
    )
    if status != 0:
        raise AssertionError(f"SelectApplication status={status:02X}")
    enc_b, status = _exchange(
        card, result, "authenticate", native_apdu(0x1A, bytes([fixture.key_number]))
    )
    if status != 0xAF or len(enc_b) != 8:
        raise AssertionError(f"Authenticate status={status:02X} bytes={len(enc_b)}")
    rnd_b = _crypt(fixture.key, bytes(8), enc_b, False)
    rnd_a = os.urandom(8)
    challenge = rnd_a + rnd_b[1:] + rnd_b[:1]
    enc_challenge = _crypt(fixture.key, enc_b, challenge, True)
    enc_a, status = _exchange(
        card, result, "authenticate_continuation", native_apdu(0xAF, enc_challenge)
    )
    if (
        status != 0
        or _crypt(fixture.key, enc_challenge[-8:], enc_a, False)
        != rnd_a[1:] + rnd_a[:1]
    ):
        raise AssertionError("card authentication proof mismatch")

    session_key = rnd_a[:4] + rnd_b[:4] + rnd_a[4:] + rnd_b[4:]
    command = (
        bytes([fixture.file_number])
        + bytes(3)
        + len(fixture.data).to_bytes(3, "little")
    )
    iv = legacy_cmac(session_key, bytes(8), b"\xbd" + command)
    response, status = _exchange(card, result, "read_data", native_apdu(0xBD, command))
    while status == 0xAF:
        continuation, status = _exchange(
            card, result, "read_data_continuation", native_apdu(0xAF)
        )
        response += continuation
    if status != 0 or len(response) != len(fixture.data) + 8:
        raise AssertionError(f"ReadData status={status:02X} bytes={len(response)}")
    payload = response[: len(fixture.data)]
    if payload != fixture.data:
        raise AssertionError("readback differs from fixture")
    iv = legacy_cmac(session_key, iv, payload + bytes([status]))
    if response[len(payload) :] != iv:
        raise AssertionError("response MAC mismatch")
    result.bytes_read = len(payload)
    return result


def run_plain_read(
    card: PcscCard, aid: bytes, file_number: int, expected: bytes
) -> PcscResult:
    result = PcscResult()
    _, status = _exchange(card, result, "select_application", native_apdu(0x5A, aid))
    if status != 0:
        raise AssertionError(f"SelectApplication status={status:02X}")
    command = bytes([file_number]) + bytes(3) + len(expected).to_bytes(3, "little")
    payload, status = _exchange(card, result, "read_data", native_apdu(0xBD, command))
    while status == 0xAF:
        continuation, status = _exchange(
            card, result, "read_data_continuation", native_apdu(0xAF)
        )
        payload += continuation
    if status != 0 or payload != expected:
        raise AssertionError(f"plain ReadData status={status:02X} bytes={len(payload)}")
    result.bytes_read = len(payload)
    return result


def run_plain_reads(reader: str, fixtures: tuple[ReadFixture, ...]) -> PcscResult:
    combined = PcscResult()
    for fixture in fixtures:
        with PcscCard(reader) as card:
            result = run_plain_read(
                card, fixture.aid, fixture.file_number, fixture.data
            )
        combined.bytes_read += result.bytes_read
        combined.frames += result.frames
        for name, timings in result.timings_ms.items():
            combined.timings_ms.setdefault(name, []).extend(timings)
    return combined
