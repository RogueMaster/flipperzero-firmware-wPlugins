import struct


def create_bmp():
    width = 10
    height = 10

    # Pixel data: 0 = nero, 1 = bianco
    # Mi Band visto di lato - capsula sottile + cinturino
    pixels = [
        [1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 0, 0, 1, 1, 1, 1],  # Cinturino superiore sottile
        [1, 1, 1, 0, 0, 0, 0, 1, 1, 1],
        [1, 1, 1, 0, 1, 1, 0, 1, 1, 1],  # Capsula piccola
        [1, 1, 1, 0, 1, 1, 0, 1, 1, 1],  # con display
        [1, 1, 1, 0, 1, 1, 0, 1, 1, 1],
        [1, 1, 1, 0, 1, 1, 0, 1, 1, 1],
        [1, 1, 1, 0, 0, 0, 0, 1, 1, 1],
        [1, 1, 1, 1, 0, 0, 1, 1, 1, 1],  # Cinturino inferiore sottile
        [1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
    ]

    # Padding per ogni riga
    row_size = ((width * 3 + 3) // 4) * 4
    pixel_data_size = row_size * height
    file_size = 54 + pixel_data_size

    # Header BMP
    bmp_header = bytearray(
        [
            0x42,
            0x4D,  # "BM"
            *struct.pack("<I", file_size),
            0x00,
            0x00,
            0x00,
            0x00,
            0x36,
            0x00,
            0x00,
            0x00,
        ]
    )

    # DIB Header
    dib_header = bytearray(
        [
            0x28,
            0x00,
            0x00,
            0x00,
            *struct.pack("<I", width),
            *struct.pack("<I", height),
            0x01,
            0x00,
            0x18,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            *struct.pack("<I", pixel_data_size),
            0x13,
            0x0B,
            0x00,
            0x00,
            0x13,
            0x0B,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        ]
    )

    # Pixel data (BGR format, bottom to top)
    pixel_data = bytearray()
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            color = 0xFF if pixels[y][x] == 1 else 0x00
            row.extend([color, color, color])
        row.extend([0x00] * (row_size - len(row)))
        pixel_data.extend(row)

    with open("../miband_nfc.bmp", "wb") as f:
        f.write(bmp_header)
        f.write(dib_header)
        f.write(pixel_data)


create_bmp()
