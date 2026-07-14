import struct
import zlib


def make_png(width, height, data):
    # Valid PNG Signature
    png_sig = b"\x89PNG\r\n\x1a\n"

    # IHDR chunk
    # Width, Height, Bit depth (1), Color type (3 - indexed), Compression, Filter, Interlace
    ihdr_data = struct.pack("!IIBBBBB", width, height, 1, 3, 0, 0, 0)
    ihdr = b"IHDR" + ihdr_data + struct.pack("!I", zlib.crc32(b"IHDR" + ihdr_data))

    # PLTE chunk (Palette) - Color 0: Transparent/Black, Color 1: Orange (Secondary) or Black (Primary)
    # Flipper icons usually:
    # 0 = Transparent (or Background)
    # 1 = Foreground (Black/Orange)
    # Let's make it simple black and white palette. 0=White/Transparent, 1=Black
    plte_data = b"\xff\xff\xff\x00\x00\x00"  # 0=White, 1=Black
    plte = b"PLTE" + plte_data + struct.pack("!I", zlib.crc32(b"PLTE" + plte_data))

    # IDAT chunk
    # Scanlines: Filter byte (0) + data
    # 10 pixels wide at 1 bit depth = 10 bits = 2 bytes per scanline (padding to byte boundary)
    raw_data = b""
    for row in data:
        # Pad row to byte boundary (10 bits -> 16 bits/2 bytes)
        # Actually 10 pixels.
        # Byte 1: P0 P1 P2 P3 P4 P5 P6 P7
        # Byte 2: P8 P9 0 0 0 0 0 0
        b1 = 0
        b2 = 0
        for i, pixel in enumerate(row):
            if i < 8:
                if pixel:
                    b1 |= 1 << (7 - i)
            else:
                if pixel:
                    b2 |= 1 << (7 - (i - 8))
        raw_data += b"\x00" + struct.pack("BB", b1, b2)  # Filter 0 + 2 bytes of data

    compressed = zlib.compress(raw_data)
    idat = b"IDAT" + compressed + struct.pack("!I", zlib.crc32(b"IDAT" + compressed))

    # IEND chunk
    iend = b"IEND" + struct.pack("!I", zlib.crc32(b"IEND"))

    return (
        png_sig
        + struct.pack("!I", len(ihdr_data))
        + ihdr
        + struct.pack("!I", len(plte_data))
        + plte
        + struct.pack("!I", len(compressed))
        + idat
        + struct.pack("!I", 0)
        + iend
    )


# 10x10 Fan Icon Pattern (1=Draw)
#   . . 1 1 1 . . . . .
#   . . 1 1 1 . . . . .
#   1 1 . 1 . 1 1 . . .
#   1 1 . 1 . 1 1 . . .
#   1 1 1 1 1 1 1 . . .
#   . . . 1 . . . 1 1 .
#   . . . 1 . . . 1 1 .
#   . . 1 1 1 . . . . .
#   . . 1 1 1 . . . . .
#   . . . 1 . . . . . .

# 10x10 Icon: Left Half Clock, Right Half Fan
icon_pattern = [
    # icon
    [1, 1, 1, 1, 1, 1, 1, 0, 0, 0],  # 0
    [1, 0, 0, 0, 0, 1, 1, 1, 1, 1],  # 1
    [0, 1, 0, 1, 0, 1, 0, 1, 0, 1],  # 2
    [0, 0, 1, 0, 1, 0, 0, 1, 1, 1],  # 3
    [0, 0, 0, 1, 0, 0, 0, 1, 0, 1],  # 4
    [0, 0, 0, 1, 0, 0, 0, 1, 1, 1],  # 5
    [0, 0, 1, 0, 1, 0, 0, 1, 0, 1],  # 6
    [0, 1, 0, 0, 0, 1, 0, 1, 1, 1],  # 7
    [1, 0, 0, 1, 0, 0, 1, 0, 1, 0],  # 8
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1],  # 9
]

with open("icon.png", "wb") as f:
    f.write(make_png(10, 10, icon_pattern))

print("Icon generated as icon.png")
