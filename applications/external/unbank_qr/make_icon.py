#!/usr/bin/env python3
"""
Generate a 10x10 monochrome PNG icon for the Unbank Flipper app.
The bitmap below is a stylized rendering of the Unbank bank-building
silhouette (top entablature, three pillars, base platform, and the
downward arrow underneath) — squeezed into the 10x10 grid that the
Flipper Application Hub requires for app icons.
"""
import struct
import zlib

WIDTH, HEIGHT = 10, 10

# '#' = black pixel (visible), '.' = white (background)
# Unbank bank silhouette: cornice + gap + architrave + 3 pillar rows
# + base bar + downward arrow narrowing to a tip.
rows = [
    "##########",   # cornice
    "..........",   # gap
    "##########",   # architrave
    "#..#..#..#",   # pillars
    "#..#..#..#",
    "#..#..#..#",
    "##########",   # base bar
    ".########.",   # arrow widest
    "...####...",
    "....##....",   # arrow tip
]

# Pack into PNG (1-bit grayscale)
# In PNG color-type 0 / bit-depth 1: bit 0 = black, bit 1 = white.
# We want '#' to be BLACK on the Flipper, so map '#' -> 0 and '.' -> 1.
raw = bytearray()
for row in rows:
    raw.append(0)  # filter: None
    padded = row + "......"  # pad each 10-bit row to 16 bits
    b1 = int(padded[0:8].replace('#', '0').replace('.', '1'), 2)
    b2 = int(padded[8:16].replace('#', '0').replace('.', '1'), 2)
    raw.append(b1)
    raw.append(b2)

compressed = zlib.compress(bytes(raw), 9)

def chunk(tag, data):
    return (
        struct.pack('>I', len(data))
        + tag
        + data
        + struct.pack('>I', zlib.crc32(tag + data))
    )

signature = b'\x89PNG\r\n\x1a\n'
ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 1, 0, 0, 0, 0))
idat = chunk(b'IDAT', compressed)
iend = chunk(b'IEND', b'')

out = signature + ihdr + idat + iend

import os
out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'icon.png')
with open(out_path, 'wb') as f:
    f.write(out)

print(f"Wrote {len(out)} bytes to {out_path}")
