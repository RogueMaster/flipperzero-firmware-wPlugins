#!/usr/bin/env python3
"""Generate the 10x10 1-bit PNG icon required by Flipper FAP manifests."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

WIDTH = 10
HEIGHT = 10


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def pack_1bit_row(row: list[int]) -> bytes:
    packed = []
    for offset in range(0, WIDTH, 8):
        value = 0
        for bit in range(8):
            value <<= 1
            x = offset + bit
            if x < WIDTH:
                value |= row[x] & 1
        packed.append(value)
    return bytes(packed)


def main() -> None:
    # 1 is white, 0 is black in 1-bit grayscale PNG.
    pixels = [[1 for _ in range(WIDTH)] for _ in range(HEIGHT)]

    def black(x: int, y: int) -> None:
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            pixels[y][x] = 0

    # Two Mayan dots.
    for x in (3, 6):
        for dx in (0, 1):
            for dy in (0, 1):
                black(x + dx, 2 + dy)

    # One Mayan bar.
    for y in (6, 7):
        for x in range(2, 8):
            black(x, y)

    raw_scanlines = b"".join(b"\x00" + pack_1bit_row(row) for row in pixels)
    ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 1, 0, 0, 0, 0)

    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(raw_scanlines, level=9))
        + png_chunk(b"IEND", b"")
    )

    Path(__file__).with_name("icon.png").write_bytes(png)


if __name__ == "__main__":
    main()
