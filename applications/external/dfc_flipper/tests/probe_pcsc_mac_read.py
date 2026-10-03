"""Verify a MACed DESFire read from a Flipper through a PC/SC reader.

Requires cryptography and a .dfc text credential with an ISO 2K3DES key.
The credential and its keys stay local; only a data hash is printed.
"""

import argparse
import ctypes
import ctypes.util
import hashlib
import os
import sys
import time
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


class IORequest(ctypes.Structure):
    _fields_ = [("protocol", ctypes.c_uint32), ("length", ctypes.c_uint32)]


if sys.platform != "darwin":
    raise SystemExit("This PC/SC probe currently supports macOS only")
lib = ctypes.CDLL(ctypes.util.find_library("PCSC"))
handle = ctypes.c_int32
dword = ctypes.c_uint32
lib.SCardEstablishContext.argtypes = [
    dword,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.POINTER(handle),
]
lib.SCardListReaders.argtypes = [
    handle,
    ctypes.c_char_p,
    ctypes.c_void_p,
    ctypes.POINTER(dword),
]
lib.SCardConnect.argtypes = [
    handle,
    ctypes.c_char_p,
    dword,
    dword,
    ctypes.POINTER(handle),
    ctypes.POINTER(dword),
]
lib.SCardTransmit.argtypes = [
    handle,
    ctypes.POINTER(IORequest),
    ctypes.c_void_p,
    dword,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.POINTER(dword),
]
lib.SCardDisconnect.argtypes = [handle, dword]
lib.SCardReleaseContext.argtypes = [handle]


def check(result):
    if result:
        raise RuntimeError(f"PCSC 0x{result & 0xFFFFFFFF:08X}")


def apdu(ins, data=b"", le=True):
    if not data:
        return bytes([0x90, ins, 0, 0, 0]) if le else bytes([0x90, ins, 0, 0])
    return bytes([0x90, ins, 0, 0, len(data)]) + data + (b"\0" if le else b"")


def crypt(key, iv, data, encrypt):
    cipher = Cipher(algorithms.TripleDES(key), modes.CBC(iv))
    operation = cipher.encryptor() if encrypt else cipher.decryptor()
    return operation.update(data) + operation.finalize()


def cmac(key, iv, message):
    zero = b"\0" * 8

    def block(value):
        return crypt(key, zero, value, True)

    def shift(value):
        number = int.from_bytes(value, "big")
        return (
            ((number << 1) & ((1 << 64) - 1)) ^ (0x1B if number >> 63 else 0)
        ).to_bytes(8, "big")

    k1 = shift(block(zero))
    k2 = shift(k1)
    complete = len(message) > 0 and len(message) % 8 == 0
    if complete:
        head, last = message[:-8], message[-8:]
        mask = k1
    else:
        head = message[: len(message) // 8 * 8]
        last = message[len(head) :] + b"\x80" + b"\0" * (7 - (len(message) - len(head)))
        mask = k2
    state = iv
    for offset in range(0, len(head), 8):
        state = block(bytes(a ^ b for a, b in zip(state, head[offset : offset + 8])))
    return block(bytes(a ^ b ^ c for a, b, c in zip(state, last, mask)))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "credential", type=Path, help=".dfc text credential loaded on the Flipper"
)
parser.add_argument("--reader", default="ACR1552", help="PC/SC reader name substring")
parser.add_argument(
    "--application", type=int, default=0, help="application index in the .dfc file"
)
parser.add_argument("--file", type=int, default=0, help="file index in the application")
parser.add_argument("--key", type=int, default=1, help="read key index")
args = parser.parse_args()

fields = dict(
    line.split(":", 1)
    for line in args.credential.read_text().splitlines()
    if ":" in line
)
fields = {name.strip(): value.strip() for name, value in fields.items()}
app_prefix = f"Application {args.application:02d}"
file_prefix = f"{app_prefix} File {args.file:02d}"
key = bytes.fromhex(fields[f"{app_prefix} Key {args.key:02d}"])
expected = bytes.fromhex(fields[f"{file_prefix} Data"])
aid = bytes.fromhex(fields[f"{app_prefix} AID"])[::-1]
file_number = int(fields[f"{file_prefix} Number"], 16)
if len(key) != 16 or len(aid) != 3 or not 0 < len(expected) < 1 << 24:
    parser.error("expected an ISO 2K3DES key, three-byte AID, and nonempty file data")
if apdu(0xAF) != bytes.fromhex("90AF000000"):
    raise AssertionError("AdditionalFrame must use a five-byte short APDU")

context = handle()
check(lib.SCardEstablishContext(2, None, None, ctypes.byref(context)))
size = dword()
check(lib.SCardListReaders(context, None, None, ctypes.byref(size)))
names = ctypes.create_string_buffer(size.value)
check(lib.SCardListReaders(context, None, names, ctypes.byref(size)))
reader = next(name for name in names.raw.split(b"\0") if args.reader.encode() in name)
card = handle()
protocol = dword()
try:
    for attempt in range(60):
        result = lib.SCardConnect(
            context, reader, 2, 3, ctypes.byref(card), ctypes.byref(protocol)
        )
        if result == 0:
            break
        if result & 0xFFFFFFFF != 0x8010000C:
            check(result)
        time.sleep(0.5)
    else:
        check(result)

    request = IORequest(protocol.value, ctypes.sizeof(IORequest))

    def exchange(command):
        response = ctypes.create_string_buffer(4096)
        length = dword(len(response))
        check(
            lib.SCardTransmit(
                card,
                ctypes.byref(request),
                command,
                len(command),
                None,
                response,
                ctypes.byref(length),
            )
        )
        value = response.raw[: length.value]
        if len(value) < 2 or value[-2] != 0x91:
            raise RuntimeError(f"Unexpected response tail: {value[-2:].hex()}")
        return value[:-2], value[-1]

    _, status = exchange(apdu(0x5A, aid))
    assert status == 0, f"SelectApplication status {status:02X}"
    enc_b, status = exchange(apdu(0x1A, bytes([args.key])))
    assert (
        status == 0xAF and len(enc_b) == 8
    ), f"Authenticate challenge status {status:02X} len {len(enc_b)}"
    rnd_b = crypt(key, b"\0" * 8, enc_b, False)
    rnd_a = os.urandom(8)
    challenge = rnd_a + rnd_b[1:] + rnd_b[:1]
    enc_challenge = crypt(key, enc_b, challenge, True)
    enc_a, status = exchange(apdu(0xAF, enc_challenge))
    assert (
        status == 0 and len(enc_a) == 8
    ), f"Authenticate response status {status:02X} len {len(enc_a)}"
    rotated_a = crypt(key, enc_challenge[-8:], enc_a, False)
    assert rotated_a == rnd_a[1:] + rnd_a[:1], "Card authentication proof mismatch"
    print("ISO 2K3DES authentication: verified")

    session_key = rnd_a[:4] + rnd_b[:4] + rnd_a[4:] + rnd_b[4:]
    iv = b"\0" * 8

    count = len(expected)
    command = bytes([file_number]) + b"\0\0\0" + count.to_bytes(3, "little")
    iv = cmac(session_key, iv, b"\xBD" + command)
    response, status = exchange(apdu(0xBD, command))
    print(
        f"ReadData count {count}: status {status:02X}, response bytes {len(response)}"
    )
    while status == 0xAF:
        continuation, status = exchange(apdu(0xAF))
        print(
            f"AdditionalFrame: status {status:02X}, response bytes {len(continuation)}"
        )
        response += continuation
    assert status == 0, f"ReadData status {status:02X}"
    assert len(response) == count + 8, "Missing or extra response MAC bytes"
    data = response[:count]
    assert data == expected, "Readback differs from credential"
    iv = cmac(session_key, iv, data + bytes([status]))
    assert response[count:] == iv, "Response MAC mismatch"
    print(f"ReadData bytes: {len(data)}")
    print(f"Credential data bytes: {len(expected)}")
    print(f"Credential SHA-256: {hashlib.sha256(expected).hexdigest()}")
    print(f"Readback SHA-256: {hashlib.sha256(data).hexdigest()}")
    print("Readback matches credential: yes")
    print("Response MAC: verified")
finally:
    if card.value:
        lib.SCardDisconnect(card, 0)
    lib.SCardReleaseContext(context)
