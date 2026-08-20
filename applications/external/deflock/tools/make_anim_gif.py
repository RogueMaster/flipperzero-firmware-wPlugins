#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt
"""
Render an asset-pack animation to a GIF for the README.

WHY A SCRIPT AND NOT A ONE-OFF EXPORT. The frames, the order and the frame rate
all live in the pack itself (meta.txt), so the GIF should be derived from them
rather than hand-assembled. Re-run this after touching an animation and the
README picture cannot drift from what the Flipper actually plays -- which is the
same failure the OUI tables had, one directory over.

The pack stores frames as 1-bit PNGs where BLACK is a lit pixel. The Flipper's
screen is the inverse: an orange backlight with dark pixels on top. So the render
maps black -> the dark pixel colour and white -> backlight orange, matching the
device screenshots in media/screenshots/ (which come off real hardware via the
same palette).

Usage:
    python tools/make_anim_gif.py                 # every animation in the pack
    python tools/make_anim_gif.py Scan            # just the ones matching "Scan"
"""

import pathlib
import re
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  python -m pip install pillow")

ROOT = pathlib.Path(__file__).resolve().parent.parent
ANIMS = ROOT / "asset_packs" / "FlipDeFlock" / "Anims"
OUT = ROOT / "media"

# The Flipper's actual screen colours, matching media/screenshots/*.png so the
# README reads as one device rather than two.
BACKLIGHT = (254, 138, 44)
PIXEL = (0, 0, 0)
SCALE = 3  # 128x64 -> 384x192, legible on GitHub without dominating the page


def read_meta(meta_path):
    """Pull frame order and rate out of a Flipper Animation meta.txt."""
    text = meta_path.read_text(encoding="utf-8", errors="replace")

    def field(name, default=None):
        m = re.search(rf"^{name}:\s*(.+)$", text, re.M)
        return m.group(1).strip() if m else default

    order_raw = field("Frames order", "")
    order = [int(n) for n in order_raw.split() if n.isdigit()]
    rate = int(field("Frame rate", "8") or 8)
    if rate <= 0:
        rate = 8
    return order, rate


def render(anim_dir):
    meta = anim_dir / "meta.txt"
    if not meta.is_file():
        print(f"  skip {anim_dir.name}: no meta.txt")
        return None

    order, rate = read_meta(meta)
    if not order:
        print(f"  skip {anim_dir.name}: no frame order")
        return None

    frames = []
    for idx in order:
        src = anim_dir / f"frame_{idx}.png"
        if not src.is_file():
            print(f"  skip {anim_dir.name}: missing {src.name}")
            return None
        mono = Image.open(src).convert("1")
        w, h = mono.size
        # Black source pixel = lit pixel on the device.
        colour = Image.new("RGB", (w, h), BACKLIGHT)
        colour.paste(Image.new("RGB", (w, h), PIXEL), mask=Image.eval(mono, lambda v: 255 - v))
        frames.append(colour.resize((w * SCALE, h * SCALE), Image.NEAREST))

    # Friendly filename for the README: L1_FlipDeFlock_Kick_128x64 -> asset-pack-kick.gif
    short = anim_dir.name.replace("L1_FlipDeFlock_", "").replace("_128x64", "").lower()
    out = OUT / f"asset-pack-{short}.gif"
    # duration is per frame in ms; loop=0 means forever.
    frames[0].save(
        out,
        save_all=True,
        append_images=frames[1:],
        duration=round(1000 / rate),
        loop=0,
        optimize=True,
    )
    print(f"  {out.relative_to(ROOT)}  {len(frames)} frames @ {rate} fps  {out.stat().st_size:,} B")
    return out


def main():
    if not ANIMS.is_dir():
        sys.exit(f"no animations at {ANIMS}")
    wanted = sys.argv[1] if len(sys.argv) > 1 else ""
    dirs = sorted(d for d in ANIMS.iterdir() if d.is_dir() and wanted.lower() in d.name.lower())
    if not dirs:
        sys.exit(f"no animation directories matching {wanted!r}")
    print(f"Rendering {len(dirs)} animation(s) from {ANIMS.relative_to(ROOT)}")
    made = [render(d) for d in dirs]
    if not any(made):
        sys.exit("nothing rendered")


if __name__ == "__main__":
    main()
