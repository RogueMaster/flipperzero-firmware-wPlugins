#!/usr/bin/env python3
# Generate 10x10 monochrome PNG icon for Maze 3D
# Design: single first-person 3D corridor perspective (raycasting view)
from PIL import Image

# 10x10 icon, mode "1": 0=black(wall), 1=white(open corridor)
W, B = 1, 0

pixels = [
    #  0  1  2  3  4  5  6  7  8  9
    [B, B, B, B, B, B, B, B, B, B],  # 0: ceiling (all black)
    [B, W, W, W, W, W, W, W, W, B],  # 1: corridor opening (wide, cols 1-8)
    [B, B, W, W, W, W, W, W, B, B],  # 2: walls step in (corridor cols 2-7)
    [B, B, W, W, W, W, W, W, B, B],  # 3: walls continue
    [B, B, B, W, W, W, W, B, B, B],  # 4: walls narrow (corridor cols 3-6)
    [B, B, B, B, B, B, B, B, B, B],  # 5: far wall (all black - end of corridor)
    [B, B, B, W, W, W, W, B, B, B],  # 6: floor walls (mirror of row 4)
    [B, B, W, W, W, W, W, W, B, B],  # 7: floor walls (mirror of row 2-3)
    [B, B, W, W, W, W, W, W, B, B],  # 8: floor walls
    [B, B, B, B, B, B, B, B, B, B],  # 9: floor (all black)
]

img = Image.new("1", (10, 10), 0)
for y, row in enumerate(pixels):
    for x, val in enumerate(row):
        img.putpixel((x, y), val)

img.save("/workspace/maze3d.png")

# Verify
print("Icon preview (## = white/corridor,   = black/wall):")
for y in range(10):
    line = ""
    for x in range(10):
        line += "##" if img.getpixel((x, y)) == 1 else "  "
    print(f"{y}: {line}")

# 10x scaled preview
preview = img.resize((100, 100), Image.NEAREST)
preview.save("/tmp/maze3d_icon_preview.png")
print("\nsaved maze3d.png + /tmp/maze3d_icon_preview.png")
