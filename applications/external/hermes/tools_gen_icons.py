#!/usr/bin/env python3
"""Generate the 10x10 1-bit FAP icon.

Hermes' caduceus, reduced to what survives ten pixels: a staff with two wires
crossing over it - which is also the TX/RX cross-over the app is about.
"""

from PIL import Image

W = H = 10
BLACK = 0
WHITE = 1

# 1 = ink. The staff runs down the middle; the two wires cross it in an X,
# echoing the caduceus and the RX/TX swap at the same time.
PIXELS = [
    "0001100000",
    "0010010000",
    "0001100000",
    "0101101010",
    "0011110100",
    "0001101000",
    "0011110100",
    "0101101010",
    "0001100000",
    "0001100000",
]


def main() -> None:
    img = Image.new("1", (W, H), WHITE)
    px = img.load()
    for y, row in enumerate(PIXELS):
        for x, ch in enumerate(row):
            if ch == "1":
                px[x, y] = BLACK
    img.save("icons/hermes_10px.png")
    print("icons/hermes_10px.png")


if __name__ == "__main__":
    main()
