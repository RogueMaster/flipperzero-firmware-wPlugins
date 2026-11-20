#!/usr/bin/env python3
"""Turn raw 128x64 framebuffers (*.fb) into PNGs that look like the Flipper's orange LCD.
Usage: png.py dir [scale]. Also writes contact.png with every frame stacked in a grid."""
import pathlib, struct, sys, zlib

d = pathlib.Path(sys.argv[1])
S = int(sys.argv[2]) if len(sys.argv) > 2 else 4
BG, INK = (255, 130, 0), (20, 14, 8)


def png(path, w, h, rows):
    raw = b"".join(b"\0" + bytes(r) for r in rows)
    chunk = (
        lambda t, data: struct.pack(">I", len(data))
        + t
        + data
        + struct.pack(">I", zlib.crc32(t + data) & 0xFFFFFFFF)
    )
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def pixels(fb):
    return [
        [(fb[y * 16 + x // 8] >> (x & 7)) & 1 for x in range(128)] for y in range(64)
    ]


frames = sorted(d.glob("*.fb"))
imgs = []
for f in frames:
    p = pixels(f.read_bytes())
    imgs.append((f.stem, p))
    rows = []
    for y in range(64 * S):
        r = []
        for x in range(128 * S):
            r += INK if p[y // S][x // S] else BG
        rows.append(r)
    png(f.with_suffix(".png"), 128 * S, 64 * S, rows)

# contact sheet, 2 columns, 3x scale with gaps
C, s, gap = 2, 3, 6
n = len(imgs)
R = (n + C - 1) // C
W, H = C * 128 * s + (C + 1) * gap, R * 64 * s + (R + 1) * gap
sheet = [[(40, 40, 40)] * W for _ in range(H)]
for i, (_, p) in enumerate(imgs):
    ox, oy = gap + (i % C) * (128 * s + gap), gap + (i // C) * (64 * s + gap)
    for y in range(64 * s):
        row = sheet[oy + y]
        for x in range(128 * s):
            row[ox + x] = INK if p[y // s][x // s] else BG
png(d / "contact.png", W, H, [[c for px in r for c in px] for r in sheet])
print(f"{n} frames -> {d}")
