#!/usr/bin/env python3
"""
Build the "hoodie kick" desktop animation for the FlipDeFlock asset pack.

    python make_kick.py <source.gif>

Output: Anims/L1_FlipDeFlock_Kick_128x64/{meta.txt, frame_0..N.png}

A tribute to @h00die, who has field-tested this project harder than anyone and
found most of the bugs worth finding.

The source animation is NOT in this repo and its path is NOT hardcoded -- pass it
in. It lives wherever the author keeps their video work, which is nobody else's
business and has no place in a public tree.

Conventions, matching make_pack.py: 128x64, 1-bit, white background (off),
black = lit pixel.

Two things that matter when converting from real artwork:

 * THRESHOLD HIGH. The art is a black silhouette on white with an orange camera.
   Orange sits near the middle of the luminance range, so a 50% threshold drops
   it to white and the camera -- the entire point of the image -- disappears.
   Anything that is not near-white becomes ink.
 * CROP, DO NOT SQUASH. The source is 16:9 and the Flipper is 2:1. Rescaling to
   fit distorts the figure; a centred crop keeps him in proportion.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ANIM = "L1_FlipDeFlock_Kick_128x64"
OUTDIR = os.path.join(HERE, "Anims", ANIM)

W, H = 128, 64
FRAMES = 24  # ~3 s at 8 fps. Each frame costs 1 KB of RAM on the device.
FRAME_RATE = 8
INK_THRESHOLD = 205  # above this is background; everything else is lit


def main(src_path):
    src = Image.open(src_path)
    n = getattr(src, "n_frames", 1)

    # Skip a fade-in: the first frames of an exported GIF are often solid black,
    # which would open the loop on a screen of every pixel lit.
    first = 0
    for f in range(n):
        src.seek(f)
        g = src.convert("L")
        if sum(g.histogram()[200:]) > g.size[0] * g.size[1] * 0.25:
            first = f
            break

    os.makedirs(OUTDIR, exist_ok=True)
    picks = [first + round(i * (n - 1 - first) / (FRAMES - 1)) for i in range(FRAMES)]
    for i, f in enumerate(picks):
        src.seek(f)
        im = src.convert("L")
        w, h = im.size
        ch = w // 2
        im = im.crop((0, (h - ch) // 2, w, (h - ch) // 2 + ch))
        im = im.resize((W, H), Image.LANCZOS)
        im = im.point(lambda v: 255 if v > INK_THRESHOLD else 0, "1")
        im.save(os.path.join(OUTDIR, f"frame_{i}.png"))

    order = " ".join(str(i) for i in range(FRAMES))
    with open(os.path.join(OUTDIR, "meta.txt"), "w", newline="\n") as fh:
        fh.write(
            "Filetype: Flipper Animation\n"
            "Version: 1\n\n"
            f"Width: {W}\n"
            f"Height: {H}\n"
            f"Passive frames: {FRAMES}\n"
            "Active frames: 0\n"
            f"Frames order: {order}\n"
            "Active cycles: 0\n"
            f"Frame rate: {FRAME_RATE}\n"
            "Duration: 0\n"
            "Active cooldown: 0\n\n"
            "Bubble slots: 0\n"
        )
    print(f"wrote {FRAMES} frames + meta.txt to {OUTDIR}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: make_kick.py <source.gif>")
    main(sys.argv[1])
