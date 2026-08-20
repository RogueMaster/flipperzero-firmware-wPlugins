#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt
"""
Stamp authorship metadata into the image assets, without touching a pixel.

WHAT THIS DOES. Writes standard PNG `tEXt` keys (Author, Copyright, License,
Source, Software) and a GIF comment block. None of it renders -- the images are
byte-for-byte identical on screen -- but `exiftool`, `identify -verbose`, Pillow,
and most image tools will show it, and it travels with a file that is copied
rather than re-exported.

WHAT THIS IS NOT. It is not tamper-proof and it is not a claim of one. Metadata
is trivially stripped by anyone who thinks to, and re-encoding drops it silently.
Its honest value is against CASUAL copying, which is precisely the case this
project has already met: a v0.41 snapshot re-uploaded verbatim under the same
name. A verbatim copy carries the metadata with it.

WHY THERE IS NO PIXEL WATERMARK. The obvious next idea is LSB steganography, and
for the two 1-bit assets it is not merely weak but actively harmful:

  asset_packs/.../frame_*.png   128x64, mode "1"
  icon.png                      10x10,  mode "1"

A 1-bit image has no least significant bit to hide in -- every bit IS the pixel.
Any payload flips visible pixels on a 128x64 monochrome LCD, corrupting the
animation the pack exists to ship. That trade is not worth making for a mark a
determined copier removes in one command.

The larger RGB assets (logo, thumbnail) could carry true LSB steg, but it would
not survive the resize, re-encode or screenshot that any reuse involves, so it
buys little over the metadata written here and costs a pixel diff nobody can
review. Provenance that actually holds up lives in the per-file SPDX headers
(tools/check_provenance.py), the published SHA256SUMS, and TRADEMARK.md.

Usage:  python tools/stamp_assets.py           # stamp everything
        python tools/stamp_assets.py --check   # verify, change nothing (CI)
"""

import sys
from pathlib import Path

try:
    from PIL import Image, PngImagePlugin
except ImportError:
    sys.exit("Pillow is required:  python -m pip install pillow")

ROOT = Path(__file__).resolve().parent.parent

AUTHOR = "ReconGrunt"
COPYRIGHT = "Copyright (c) 2026 ReconGrunt"
LICENSE = "GPL-3.0-or-later"
SOURCE = "https://github.com/ReconGrunt/FlipDeFlock"
SOFTWARE = "FlipDeFlock"

# Only first-party artwork. Screenshots are captured output and get the same
# treatment; vendored or third-party images would not belong here.
PATTERNS = [
    "media/*.png",
    "media/*.gif",
    "media/screenshots/*.png",
    "icon.png",
    "asset_packs/**/*.png",
]


def targets():
    seen = set()
    for pat in PATTERNS:
        for p in sorted(ROOT.glob(pat)):
            if p.is_file() and p not in seen:
                seen.add(p)
                yield p


def stamped(p: Path) -> bool:
    """True if this file already carries our authorship."""
    try:
        with Image.open(p) as im:
            info = im.info
            if p.suffix.lower() == ".gif":
                c = info.get("comment", b"")
                return AUTHOR.encode() in (c if isinstance(c, bytes) else c.encode())
            return info.get("Author") == AUTHOR
    except Exception:
        return False


def stamp(p: Path) -> bool:
    """Write metadata in place. Returns True if the file was changed."""
    if stamped(p):
        return False
    with Image.open(p) as im:
        im.load()
        fmt = (im.format or "").upper()
        if fmt == "GIF":
            # GIF has no key/value text chunk; the comment block is the standard
            # place, and Pillow round-trips animation frames via save_all.
            frames = []
            try:
                while True:
                    frames.append(im.copy())
                    im.seek(im.tell() + 1)
            except EOFError:
                pass
            comment = f"{COPYRIGHT} | {LICENSE} | {SOURCE}".encode()
            frames[0].save(
                p,
                save_all=True,
                append_images=frames[1:],
                duration=im.info.get("duration", 125),
                loop=im.info.get("loop", 0),
                comment=comment,
                optimize=True,
            )
            return True

        meta = PngImagePlugin.PngInfo()
        # Re-carry any existing text so stamping is additive, never lossy.
        for k, v in im.info.items():
            if isinstance(v, str) and k not in ("Author", "Copyright", "License", "Source", "Software"):
                meta.add_text(k, v)
        meta.add_text("Author", AUTHOR)
        meta.add_text("Copyright", COPYRIGHT)
        meta.add_text("License", LICENSE)
        meta.add_text("Source", SOURCE)
        meta.add_text("Software", SOFTWARE)
        # optimize=True keeps 1-bit frames 1-bit; the pixels are untouched either way.
        im.save(p, pnginfo=meta, optimize=True)
        return True


def main():
    check_only = "--check" in sys.argv
    files = list(targets())
    if not files:
        sys.exit("no assets matched")

    print(f"{'Checking' if check_only else 'Stamping'} {len(files)} asset(s) with {AUTHOR} authorship")
    unstamped, changed = [], 0
    for p in files:
        rel = p.relative_to(ROOT)
        if check_only:
            if not stamped(p):
                unstamped.append(rel)
        elif stamp(p):
            changed += 1
            print(f"  stamped {rel}")

    if check_only:
        if unstamped:
            print(f"\n  {len(unstamped)} asset(s) missing authorship metadata:")
            for rel in unstamped:
                print(f"        {rel}")
            print("\nFAIL: run  python tools/stamp_assets.py")
            return 1
        print(f"  OK   all {len(files)} assets carry {AUTHOR} authorship")
        print("\nRESULT: PASS")
        return 0

    print(f"  {changed} changed, {len(files) - changed} already stamped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
