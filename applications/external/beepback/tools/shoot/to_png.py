#!/usr/bin/env python3
"""Turn the rendered 128x64 frames into the PNGs the Apps Catalog wants.

The catalog's bundler accepts a screenshot at exactly 4x or 8x the
Flipper's 128x64 and rejects anything else, then maps every pixel darker
than (15,15,15) to black and everything else to transparent. So the only
things that matter are the size and which pixels are dark; the colours
below are the Flipper's own screen, so what a moderator sees looks like
what a photograph of the device looks like.
"""
import sys
from pathlib import Path

from PIL import Image

SCALE = 4
INK = (0, 0, 0)
SCREEN = (255, 130, 0)


def load_pbm(path):
    tokens = path.read_text().split()
    assert tokens[0] == "P1", path
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    assert len(bits) == w * h, f"{path}: {len(bits)} bits for {w}x{h}"
    return w, h, bits


def main(out_dir):
    out = Path(out_dir)
    made = 0
    for pbm in sorted(out.glob("*.pbm")):
        w, h, bits = load_pbm(pbm)
        img = Image.new("RGB", (w, h))
        img.putdata([INK if b == "1" else SCREEN for b in bits])
        img = img.resize((w * SCALE, h * SCALE), Image.Resampling.NEAREST)
        png = pbm.with_suffix(".png")
        img.save(png)
        pbm.unlink()
        print(f"{png.name}  {img.width}x{img.height}")
        made += 1
    if not made:
        print("no frames rendered", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
