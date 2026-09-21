#!/usr/bin/env python3
"""Compose the "straight off the device" strip for the README.

Everything else in images/ is drawn by tools_gen_mockups.py from the views'
own layout constants. These four are not drawn at all - they are qFlipper
captures from a real unit, at 512x256 (4x the 128x64 screen), and that is the
whole point of the panel: the mockups prove the layout, this proves the thing
exists and works.

Only captures that still match the current source belong here. Each was checked
by downsampling back to device pixels and comparing inked rows against the view
constants; see docs/catalog/README.md for the per-file status.

    python3 tools_gen_device.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "images")

# True black, per the house style - not the blue-black this repo used to use.
BG = (0, 0, 0)
CAPTION = (150, 150, 150)
RULE = (40, 40, 40)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"

PANELS = [
    ("device_sweep_reader.png", "SWEEP", "on a live reader, meter pegged"),
    ("device_sweep_idle.png", "SWEEP", "quiet room, nothing found yet"),
    ("device_watch_alarm.png", "WATCH", "a reader turned up while you were away"),
    ("device_watch_quiet.png", "WATCH", "standing guard, one contact logged"),
]


def build():
    ims = [Image.open(os.path.join(OUT, f)).convert("RGB") for f, _, _ in PANELS]
    w, h = ims[0].size
    pad, gutter, cap_h = 34, 30, 44

    cols, rows = 2, 2
    W = pad * 2 + w * cols + gutter * (cols - 1)
    H = pad * 2 + (h + cap_h) * rows + gutter * (rows - 1)

    sheet = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(sheet)
    f_label = ImageFont.truetype(MONO, 22)
    f_cap = ImageFont.truetype(MONO, 19)

    for i, (im, (_, label, caption)) in enumerate(zip(ims, PANELS)):
        r, c = divmod(i, cols)
        x = pad + c * (w + gutter)
        y = pad + r * (h + cap_h + gutter)
        sheet.paste(im, (x, y))
        # a hairline under the screen, then the caption - no frames, no shadows
        d.line([x, y + h + 13, x + w, y + h + 13], fill=RULE, width=2)
        d.text((x, y + h + 22), label, font=f_label, fill=(230, 230, 230))
        tw = d.textlength(label, font=f_label)
        d.text((x + tw + 14, y + h + 24), caption, font=f_cap, fill=CAPTION)

    path = os.path.join(OUT, "device.png")
    sheet.save(path)
    print("wrote", path, sheet.size)


if __name__ == "__main__":
    build()
