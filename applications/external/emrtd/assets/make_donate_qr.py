#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Filip Sedivy
"""Draw the QR code the Donate screen shows.

The code carries exactly the address the README's Support section links to,
and nothing else. The Flipper has no QR encoder in its SDK, and the address
never changes, so the code is encoded once, here, and compiled into the
application as an icon rather than built at run time.

Run with:

    uv run --with segno --with pillow --with zxing-cpp python assets/make_donate_qr.py

Output:
    images/EmrtdDonateQr_58x58.png   the code, two pixels per module, one bit
"""

from pathlib import Path

import segno
import zxingcpp
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent

URL = "https://buymeacoffee.com/filipsedivy"

# The address stays exactly as the README spells it, in byte mode. Upper
# casing the scheme and the host and encoding them alphanumerically would fit
# a version 2 code at level L, but Android matches the scheme of an intent
# case-sensitively, and some scanners hand "HTTPS://" to a system that then
# finds no browser for it.
#
# Thirty-six bytes need version 3, 29 modules a side, which still holds them
# at error correction level M. At two pixels a module that is 58 of the
# screen's 64 rows: one pixel a module would be a code 7 mm across on the
# Flipper's display, too small for a phone to focus on.
VERSION = 3
ERROR = "M"
SCALE = 2

# The mask segno's penalty rules pick for this address. It is pinned so that
# a later segno cannot redraw the same address as a different valid code and
# leave a diff in images/ that changes nothing.
MASK = 2

# What the Donate screen leaves around the code: three pixels above, below
# and to the right of it (scenes/emrtd_scene_donate.c). A module and a half,
# against the four a printed code gets.
SCREEN_MARGIN = 3

INK = 0
PAPER = 1


def write_code(path):
    """Write the code as a one bit PNG, with no quiet zone of its own.

    The quiet zone is left to the screen: the scene places the code so that
    the paper around it is part of the layout, where the margins can be read
    off the coordinates instead of hidden inside the image.
    """
    code = segno.make_qr(
        URL, version=VERSION, error=ERROR, mode="byte", mask=MASK, boost_error=False
    )
    assert code.version == VERSION, f"encoded as version {code.version}, not {VERSION}"
    assert code.error == ERROR, f"encoded at level {code.error}, not {ERROR}"

    modules = code.symbol_size(border=0)[0]
    size = modules * SCALE
    image = Image.new("1", (size, size), PAPER)
    for y, row in enumerate(code.matrix):
        for x, dark in enumerate(row):
            if dark:
                for dy in range(SCALE):
                    for dx in range(SCALE):
                        image.putpixel((x * SCALE + dx, y * SCALE + dy), INK)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)

    reopened = Image.open(path)
    assert reopened.mode == "1", f"{path}: mode is {reopened.mode}, not 1"
    assert reopened.size == (size, size), f"{path}: size is {reopened.size}"
    assert not reopened.info, f"{path}: carries metadata {sorted(reopened.info)}"
    assert path.stem.endswith(f"_{size}x{size}"), (
        f"{path}: the name does not say {size}x{size}"
    )

    # Read the written file back inside no more paper than the screen gives
    # it. An encoder bug, a margin too thin to find the code in, or a code
    # drawn light on dark - which a light frame around it cannot be read in -
    # fails here rather than in front of somebody's camera. try_invert is on
    # by default and would only try the other polarity; it stays off, so the
    # code is read the one way the screen shows it.
    framed = Image.new("L", (size + 2 * SCREEN_MARGIN,) * 2, 255)
    framed.paste(reopened.convert("L"), (SCREEN_MARGIN, SCREEN_MARGIN))
    framed = framed.resize((framed.width * 4, framed.height * 4), Image.NEAREST)
    results = zxingcpp.read_barcodes(
        framed, formats=zxingcpp.BarcodeFormat.QRCode, try_invert=False
    )
    decoded = [result.text for result in results]
    assert decoded == [URL], f"{path}: reads back as {decoded}, not {URL!r}"

    print(
        f"  {path.relative_to(ROOT)}  {size}x{size}, 1 bit, "
        f"version {code.version}-{code.error}, mask {code.mask}, reads back"
    )


def main():
    print("Drawing the Donate QR code:")
    write_code(ROOT / "images" / "EmrtdDonateQr_58x58.png")


if __name__ == "__main__":
    main()
