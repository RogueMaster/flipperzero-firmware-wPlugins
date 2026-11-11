#!/usr/bin/env python3
"""Write hamcram.png, the 10x10 1-bit app icon (a radio tower with waves).

Edit the pixel art below and re-run. Standard library only.
"""

import struct
import zlib
from pathlib import Path

ART = """
.#......#.
#..#..#..#
#.#.##.#.#
#..#..#..#
.#..##..#.
....##....
....##....
...#..#...
..#....#..
.########.
"""


def chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def main() -> None:
    rows = ART.strip().splitlines()
    assert len(rows) == 10 and all(len(row) == 10 for row in rows), "icon must be 10x10"
    raw = b""
    for row in rows:
        # 1-bit greyscale: 0 = black, 1 = white; rows are padded to whole bytes
        bits = int("".join("0" if c == "#" else "1" for c in row).ljust(16, "0"), 2)
        raw += b"\x00" + struct.pack(">H", bits)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 10, 10, 1, 0, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    out = Path(__file__).resolve().parent.parent / "hamcram.png"
    out.write_bytes(png)
    print(f"wrote {out.name} ({len(png)} bytes)")


if __name__ == "__main__":
    main()
