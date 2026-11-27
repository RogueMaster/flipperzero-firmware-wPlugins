"""Capture Flipper Zero screens over the RPC protocol, rendered like qFlipper
screenshots (512x256, orange + black). Usage:
  flipshot.py OUTDIR name:keys name:keys ...
keys: comma list of up,down,left,right,ok,back (empty = capture current screen).
"""

import os, sys, time
import serial
from PIL import Image

PORT = "COM12"
KEYS = {"up": 0, "down": 1, "right": 2, "left": 3, "ok": 4, "back": 5}
ORANGE = (254, 138, 44)
BLACK = (0, 0, 0)


def varint(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def field_bytes(num, payload):
    return varint((num << 3) | 2) + varint(len(payload)) + payload


def field_varint(num, val):
    return varint(num << 3) + varint(val) if val else b""


class Rpc:
    def __init__(self):
        self.s = serial.Serial(PORT, 230400, timeout=0.05)
        time.sleep(0.2)
        self.s.read(65536)
        self.s.write(b"start_rpc_session\r")
        time.sleep(0.5)
        self.s.read(65536)  # echo
        self.cid = 1
        self.buf = b""
        self.frame = None

    def send(self, content_field, payload):
        msg = field_varint(1, self.cid) + field_bytes(content_field, payload)
        self.cid += 1
        self.s.write(varint(len(msg)) + msg)

    def pump(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            self.buf += self.s.read(8192)
            while True:
                n, shift, i = 0, 0, 0
                ok = False
                while i < len(self.buf):
                    b = self.buf[i]
                    n |= (b & 0x7F) << shift
                    shift += 7
                    i += 1
                    if not b & 0x80:
                        ok = True
                        break
                if not ok or len(self.buf) < i + n:
                    break
                self.handle(self.buf[i : i + n])
                self.buf = self.buf[i + n :]

    @staticmethod
    def fields(msg):
        i = 0
        while i < len(msg):
            key, shift = 0, 0
            while True:
                b = msg[i]
                i += 1
                key |= (b & 0x7F) << shift
                shift += 7
                if not b & 0x80:
                    break
            num, wt = key >> 3, key & 7
            if wt == 0:
                v, shift = 0, 0
                while True:
                    b = msg[i]
                    i += 1
                    v |= (b & 0x7F) << shift
                    shift += 7
                    if not b & 0x80:
                        break
                yield num, v
            elif wt == 2:
                ln, shift = 0, 0
                while True:
                    b = msg[i]
                    i += 1
                    ln |= (b & 0x7F) << shift
                    shift += 7
                    if not b & 0x80:
                        break
                yield num, msg[i : i + ln]
                i += ln
            else:
                return

    def handle(self, msg):
        for num, val in self.fields(msg):
            if num == 22:  # gui_screen_frame
                for n2, v2 in self.fields(val):
                    if n2 == 1 and len(v2) == 1024:
                        self.frame = v2

    def press(self, key):
        k = KEYS[key]
        for t in (0, 2, 1):  # PRESS, SHORT, RELEASE
            self.send(23, field_varint(1, k) + field_varint(2, t))
            self.pump(0.05)

    def save(self, path):
        img = Image.new("RGB", (128, 64), ORANGE)
        px = img.load()
        for y in range(64):
            for x in range(128):
                if (self.frame[(y // 8) * 128 + x] >> (y % 8)) & 1:
                    px[x, y] = BLACK
        img.resize((512, 256), Image.NEAREST).save(path)

    def close(self):
        self.send(21, b"")
        self.pump(0.3)
        self.send(19, b"")
        self.pump(0.3)
        self.s.close()


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    r = Rpc()
    r.send(20, b"")  # start screen stream
    r.pump(1.0)
    for spec in sys.argv[2:]:
        name, _, keys = spec.partition(":")
        for k in filter(None, keys.split(",")):
            r.press(k)
            r.pump(0.4)
        r.pump(1.2)
        if r.frame is None:
            print("no frame for", name)
            continue
        r.save(os.path.join(outdir, name + ".png"))
        print("saved", name)
    r.close()


if __name__ == "__main__":
    main()
