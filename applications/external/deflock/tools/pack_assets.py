#!/usr/bin/env python3
"""
Pack a Flipper asset pack's .png frames into the .bm files the firmware reads.

    python tools/pack_assets.py <src_pack_dir> <out_pack_dir>

The repo keeps frames as PNG because that is what a human can open, diff and
regenerate. The firmware wants .bm. This is the step between, and it runs in CI
so every release ships an installable pack rather than a directory of sources
nobody can use.

FORMAT. A .bm is a 1-bit XBM bitmap, optionally heatshrink-compressed:

    compressed:    01 00 <len_lo> <len_hi> <heatshrink(w=8, l=4) data>
    uncompressed:  00 <raw xbm bytes>

with the compressed form used only when it is actually smaller. This mirrors
flipper/assets/icon.py in the firmware SDK exactly, deliberately: the encoding is
easy to get subtly wrong in a way that still produces a plausible-looking file
which renders as garbage on the device. Validated by re-packing the pack's
existing frames and diffing against the .bm the SDK produced -- 16/16 identical,
byte for byte.
"""
import io
import os
import shutil
import sys

import heatshrink2
from PIL import Image, ImageOps


def png_to_bm(path):
    with Image.open(path) as im:
        with io.BytesIO() as out:
            ImageOps.invert(im.convert("1")).save(out, format="XBM")
            xbm = out.getvalue()

    f = io.StringIO(xbm.decode().strip())
    f.readline()  # width
    f.readline()  # height
    body = f.read().strip().replace("\n", "").replace(" ", "").split("=")[1][:-1]
    raw = bytearray.fromhex(body[1:-1].replace(",", " ").replace("0x", ""))

    enc = bytearray(heatshrink2.compress(bytes(raw), window_sz2=8, lookahead_sz2=4))
    enc = bytearray([len(enc) & 0xFF, len(enc) >> 8]) + enc
    # Only worth it if it actually wins, counting both headers.
    return b"\x01\x00" + enc if len(enc) + 2 < len(raw) + 1 else b"\x00" + raw


def main(src, dst):
    anims_src = os.path.join(src, "Anims")
    anims_dst = os.path.join(dst, "Anims")
    os.makedirs(anims_dst, exist_ok=True)
    shutil.copy(
        os.path.join(anims_src, "manifest.txt"), os.path.join(anims_dst, "manifest.txt")
    )

    total = 0
    for name in sorted(os.listdir(anims_src)):
        d = os.path.join(anims_src, name)
        if not os.path.isdir(d):
            continue
        o = os.path.join(anims_dst, name)
        os.makedirs(o, exist_ok=True)
        shutil.copy(os.path.join(d, "meta.txt"), os.path.join(o, "meta.txt"))
        n = 0
        while os.path.exists(os.path.join(d, f"frame_{n}.png")):
            with open(os.path.join(o, f"frame_{n}.bm"), "wb") as fh:
                fh.write(png_to_bm(os.path.join(d, f"frame_{n}.png")))
            n += 1
        print(f"{name}: {n} frames")
        total += n

    # Anything outside Anims/ (Icons/, etc.) is copied through untouched.
    for entry in os.listdir(src):
        if entry in ("Anims",) or entry.endswith(".py"):
            continue
        s = os.path.join(src, entry)
        t = os.path.join(dst, entry)
        if os.path.isdir(s):
            shutil.copytree(s, t, dirs_exist_ok=True)
        else:
            shutil.copy(s, t)

    if total == 0:
        sys.exit("no frames packed -- refusing to ship an empty asset pack")
    print(f"packed {total} frames into {dst}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: pack_assets.py <src_pack_dir> <out_pack_dir>")
    main(sys.argv[1], sys.argv[2])
