"""Lay the simulator's PBM frames out on one PNG, scaled up, in Flipper
LCD colours. Usage: python3 sheet.py out/ sheet.png"""

import os
import struct
import sys
import zlib

src, dst = sys.argv[1], sys.argv[2]
names = sorted(n for n in os.listdir(src) if n.endswith(".pbm"))
S, W, H, COLS, PAD = 3, 128, 64, 2, 6
BG, FG, GAP = (255, 147, 41), (20, 12, 0), (40, 40, 40)

frames = []
for n in names:
    data = open(os.path.join(src, n), "rb").read()
    body = data.split(b"\n", 2)[2]
    frames.append(body)

rows = (len(frames) + COLS - 1) // COLS
SW, SH = COLS * (W * S + PAD) + PAD, rows * (H * S + PAD) + PAD
img = [[GAP] * SW for _ in range(SH)]
for i, body in enumerate(frames):
    ox = PAD + (i % COLS) * (W * S + PAD)
    oy = PAD + (i // COLS) * (H * S + PAD)
    for y in range(H):
        for x in range(W):
            on = body[y * 16 + x // 8] & (0x80 >> (x % 8))
            c = FG if on else BG
            for dy in range(S):
                row = img[oy + y * S + dy]
                for dx in range(S):
                    row[ox + x * S + dx] = c

raw = b"".join(b"\x00" + bytes(v for px in r for v in px) for r in img)


def chunk(t, d):
    return (
        struct.pack(">I", len(d))
        + t
        + d
        + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    )


png = b"\x89PNG\r\n\x1a\n" + chunk(
    b"IHDR", struct.pack(">IIBBBBB", SW, SH, 8, 2, 0, 0, 0)
)
png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
open(dst, "wb").write(png)
print("\n".join(f"{i}: {n}" for i, n in enumerate(names)))
