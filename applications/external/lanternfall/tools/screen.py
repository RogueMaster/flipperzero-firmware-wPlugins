# Grab one frame of the Flipper's screen over the RPC protocol (the stream qFlipper uses) and save it as
# qFlipper does: 512x256, black on orange (254,138,44). Usage: python3 tools/screen.py out.png
import serial, time, sys, zlib, struct

out = sys.argv[1]
s = serial.Serial("/dev/cu.usbmodemflip_Leckr1ke1", 230400, timeout=0.5)
time.sleep(0.3)
s.write(b"\x05\x08\x03\x9a\x01\x00")
time.sleep(0.5)
s.write(b"\r")
time.sleep(0.3)
s.read(65536)  # leave any old RPC session
s.write(b"start_rpc_session\r")
time.sleep(0.5)
s.read(65536)


def varint(n):
    b = b""
    while True:
        x = n & 0x7F
        n >>= 7
        b += bytes([x | (0x80 if n else 0)])
        if not n:
            return b


def send(cmd_id, field):
    msg = b"\x08" + varint(cmd_id) + varint((field << 3) | 2) + b"\x00"
    s.write(varint(len(msg)) + msg)


send(1, 20)  # gui_start_screen_stream_request
buf = b""
t = time.time()
frame = None


def rv(b, i):
    n = sh = 0
    while True:
        x = b[i]
        i += 1
        n |= (x & 0x7F) << sh
        sh += 7
        if not x & 0x80:
            return n, i


while time.time() - t < 4 and frame is None:
    buf += s.read(4096)
    while buf:
        try:
            ln, i = rv(buf, 0)
        except IndexError:
            break
        if len(buf) < i + ln:
            break
        m = buf[i : i + ln]
        buf = buf[i + ln :]
        j = 0
        while j < len(m):
            tag, j = rv(m, j)
            f, wt = tag >> 3, tag & 7
            if wt == 0:
                _, j = rv(m, j)
            elif wt == 2:
                l, j = rv(m, j)
                v = m[j : j + l]
                j += l
                if f == 22:
                    k = 0
                    while k < len(v):
                        t2, k = rv(v, k)
                        if t2 >> 3 == 1 and t2 & 7 == 2:
                            l2, k = rv(v, k)
                            frame = v[k : k + l2]
                            k += l2
                        elif t2 & 7 == 0:
                            _, k = rv(v, k)
                        else:
                            break
            else:
                break
send(2, 21)  # stop stream
time.sleep(0.3)
s.write(b"\x05\x08\x03\x9a\x01\x00")  # stop_session: back to the text CLI
time.sleep(1.0)
s.read(65536)
if not frame:
    print("no frame")
    sys.exit(1)
W, H, SC = 128, 64, 4
rows = []
for y in range(H * SC):
    r = bytearray([0])
    for x in range(W * SC):
        px, py = x // SC, y // SC
        on = frame[(py // 8) * 128 + px] >> (py % 8) & 1
        r += bytes(
            (0, 0, 0) if on else (254, 138, 44)
        )  # qFlipper's palette, at its 4x export size
    rows.append(bytes(r))


def chunk(k, d):
    return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d))


open(out, "wb").write(
    b"\x89PNG\r\n\x1a\n"
    + chunk(b"IHDR", struct.pack(">IIBBBBB", W * SC, H * SC, 8, 2, 0, 0, 0))
    + chunk(b"IDAT", zlib.compress(b"".join(rows)))
    + chunk(b"IEND", b"")
)
print("frame", len(frame))
