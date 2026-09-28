import ctypes
import ctypes.util
import hashlib

DWORD = ctypes.c_uint32
HANDLE = ctypes.c_int32


class IORequest(ctypes.Structure):
    _fields_ = [("protocol", DWORD), ("length", DWORD)]


lib = ctypes.CDLL(ctypes.util.find_library("PCSC"))
lib.SCardEstablishContext.argtypes = [
    DWORD,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.POINTER(HANDLE),
]
lib.SCardEstablishContext.restype = ctypes.c_int32
lib.SCardListReaders.argtypes = [
    HANDLE,
    ctypes.c_char_p,
    ctypes.c_void_p,
    ctypes.POINTER(DWORD),
]
lib.SCardListReaders.restype = ctypes.c_int32
lib.SCardConnect.argtypes = [
    HANDLE,
    ctypes.c_char_p,
    DWORD,
    DWORD,
    ctypes.POINTER(HANDLE),
    ctypes.POINTER(DWORD),
]
lib.SCardConnect.restype = ctypes.c_int32
lib.SCardTransmit.argtypes = [
    HANDLE,
    ctypes.POINTER(IORequest),
    ctypes.c_void_p,
    DWORD,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.POINTER(DWORD),
]
lib.SCardTransmit.restype = ctypes.c_int32
lib.SCardDisconnect.argtypes = [HANDLE, DWORD]
lib.SCardReleaseContext.argtypes = [HANDLE]


def check(result):
    if result:
        raise RuntimeError(f"PCSC 0x{result & 0xFFFFFFFF:08X}")


context = HANDLE()
check(lib.SCardEstablishContext(2, None, None, ctypes.byref(context)))
size = DWORD()
check(lib.SCardListReaders(context, None, None, ctypes.byref(size)))
names = ctypes.create_string_buffer(size.value)
check(lib.SCardListReaders(context, None, names, ctypes.byref(size)))
reader = next(name for name in names.raw.split(b"\0") if b"ACR1552" in name)
card = HANDLE()
protocol = DWORD()
check(
    lib.SCardConnect(context, reader, 2, 3, ctypes.byref(card), ctypes.byref(protocol))
)
request = IORequest(protocol.value, ctypes.sizeof(IORequest))


def exchange(command):
    response = ctypes.create_string_buffer(258)
    length = DWORD(len(response))
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
    return response.raw[: length.value]


try:
    selected = exchange(bytes.fromhex("905A00000301000000"))
    assert selected == bytes.fromhex("9100"), selected.hex()
    response = exchange(bytes.fromhex("90BD00000700000000000800"))
    chunks = []
    frames = 0
    while True:
        frames += 1
        assert len(response) >= 2 and response[-2] == 0x91, response.hex()
        chunks.append(response[:-2])
        if response[-1] == 0:
            break
        assert response[-1] == 0xAF and frames < 64, response.hex()
        response = exchange(bytes.fromhex("90AF000000"))
    payload = b"".join(chunks)
    assert len(payload) == 2048, len(payload)
    assert payload == bytes(2048), hashlib.sha256(payload).hexdigest()
    print(f"ReadData OK: {len(payload)} bytes in {frames} frames")
finally:
    lib.SCardDisconnect(card, 0)
    lib.SCardReleaseContext(context)
