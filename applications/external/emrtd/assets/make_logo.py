#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Filip Sedivy
"""Draw the eMRTD mark, everywhere it is needed, from one definition.

The mark is the contact plate of the chip module inside a travel document:
a plate divided into six pads. It is defined once, on a ten by ten grid,
because that is the hardest place it has to work - the Flipper launcher shows
the application icon at exactly that size, in one bit colour, with no
antialiasing to hide behind. Everything larger is the same grid scaled by a
whole number, so an edge is never half a pixel anywhere.

Run with:

    uv run --with pillow python assets/make_logo.py

Outputs:
    images/emrtd_10px.png      the application icon, 10x10, one bit
    images/EmrtdChip_24x24.png the same mark for the reading screen
    assets/logo.svg            the mark and the wordmark, as vector art
    assets/logo.png            a raster export of the same
    assets/logo.txt            the mark as text, for a terminal or a README
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent

# The mark on its ten by ten grid. The plate is an eight by eight square with
# a one pixel margin; four notches cut it into six pads. Nothing drawn is
# thinner than two pixels, which is what keeps it from turning into a dither
# pattern on the device.
GRID = 10
PLATE = (1, 1, 8, 8)  # left, top, right, bottom, inclusive
NOTCHES = [
    (3, 2, 3, 3),  # upper left gap
    (6, 2, 6, 3),  # upper right gap
    (3, 6, 3, 7),  # lower left gap
    (6, 6, 6, 7),  # lower right gap
]

INK = 0
PAPER = 1

# The wordmark, and the advance width of every character it uses, in em.
# The figures are Arial's, which is metric compatible with Helvetica and
# with Liberation Sans, so this one table describes the wordmark on macOS,
# on Windows and on Linux alike. They are here because the drawing has to
# know how wide the type is before it can decide how wide the picture is -
# guessing that number is what once cropped the D off the end.
WORDMARK = "eMRTD"
TAGLINE = ("electronic document reader", "for Flipper Zero")

ADVANCE_BOLD = {
    "D": 0.7222, "M": 0.8330, "R": 0.7222, "T": 0.6108, "e": 0.5562,
}
ADVANCE_REGULAR = {
    " ": 0.2778, "F": 0.6108, "Z": 0.6108, "a": 0.5562, "c": 0.5000,
    "d": 0.5562, "e": 0.5562, "f": 0.2778, "i": 0.2222, "l": 0.2222,
    "m": 0.8330, "n": 0.5562, "o": 0.5562, "p": 0.5562, "r": 0.3330,
    "s": 0.5000, "t": 0.2778, "u": 0.5562,
}
CAP_HEIGHT = 0.716  # em, the height of a capital, Arial and Helvetica alike
DESCENDER = 0.212  # em, how far below the baseline a p reaches

FONT_STACK = "Helvetica Neue, Helvetica, Arial, sans-serif"

# What the logo is drawn in when nothing else says. GitHub renders this file
# as an image, and in that context currentColor is black - which disappears
# against a dark README - so a colour is set here and swapped with the
# reader's colour scheme. The selector is svg:root, which matches the svg
# only while it is the root of a document of its own. Pasted inline into a
# page it matches nothing, and the mark inherits the colour of the text
# around it; a bare :root would instead match that page's html element and
# repaint everything on it.
INK_LIGHT = "#1f2328"
INK_DARK = "#e6edf3"


def mark_pixels():
    """The mark as a set of the grid cells that are inked."""
    left, top, right, bottom = PLATE
    cells = {
        (x, y)
        for x in range(left, right + 1)
        for y in range(top, bottom + 1)
    }
    for nx0, ny0, nx1, ny1 in NOTCHES:
        for x in range(nx0, nx1 + 1):
            for y in range(ny0, ny1 + 1):
                cells.discard((x, y))
    return cells


def write_bitmap(path, scale, margin=0):
    """Write the mark as a one bit PNG, scaled by a whole number."""
    size = GRID * scale + 2 * margin
    image = Image.new("1", (size, size), PAPER)
    draw = ImageDraw.Draw(image)
    for x, y in mark_pixels():
        x0 = margin + x * scale
        y0 = margin + y * scale
        draw.rectangle([x0, y0, x0 + scale - 1, y0 + scale - 1], fill=INK)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)

    reopened = Image.open(path)
    assert reopened.mode == "1", f"{path}: mode is {reopened.mode}, not 1"
    assert reopened.size == (size, size), f"{path}: size is {reopened.size}"
    print(f"  {path.relative_to(ROOT)}  {reopened.size[0]}x{reopened.size[1]}, 1 bit")


def merged_rectangles():
    """The mark as a few rectangles rather than a hundred cells.

    Rows of adjacent cells are joined, and identical rows stacked, so the
    raster export draws a handful of shapes instead of a hundred cells. The
    vector form is drawn by mark_outline instead, as one path.
    """
    cells = mark_pixels()
    runs_by_row = {}
    for y in range(GRID):
        runs = []
        x = 0
        while x < GRID:
            if (x, y) in cells:
                start = x
                while x < GRID and (x, y) in cells:
                    x += 1
                runs.append((start, x - start))
            else:
                x += 1
        if runs:
            runs_by_row[y] = runs

    rects = []
    y = 0
    while y < GRID:
        runs = runs_by_row.get(y)
        if runs is None:
            y += 1
            continue
        height = 1
        while runs_by_row.get(y + height) == runs:
            height += 1
        for start, width in runs:
            rects.append((start, y, width, height))
        y += height
    return rects


def text_width(text, table, size, tracking=0.0):
    """How wide a string sets, in vector units.

    Tracking is counted between the letters and not after the last one,
    which is the difference between the type fitting the box and the box
    being one letter too narrow.
    """
    natural = sum(table[character] for character in text) * size
    return natural + tracking * max(len(text) - 1, 0)


def mark_outline(unit):
    """The mark as one path: the plate, with the notches cut out of it.

    The raster forms are drawn cell by cell, but the vector form is a single
    even-odd path rather than a row of rectangles that share their edges.
    Two abutting rectangles are antialiased one at a time, and where a
    README scales the logo to some width in pixels the scale is fractional,
    so the coverage either side of a shared edge does not add back up to one
    and the seam shows as a pale hairline across the plate.
    """

    def box(x0, y0, x1, y1):
        """One closed rectangle, from inclusive cell indices."""
        x_start, y_start = x0 * unit, y0 * unit
        x_end, y_end = (x1 + 1) * unit, (y1 + 1) * unit
        return f"M{x_start} {y_start}H{x_end}V{y_end}H{x_start}Z"

    return "".join([box(*PLATE)] + [box(*notch) for notch in NOTCHES])


def write_svg(path, unit=24, gap=18, pad=24):
    """The mark beside the wordmark, in vector units.

    The width of the picture is measured from the type rather than assumed,
    and every line is given a textLength, so a reader whose machine has none
    of the fonts in the stack still gets the wordmark inside the frame
    instead of running off the edge of it.
    """
    mark_size = GRID * unit
    text_x = mark_size + gap
    height = mark_size

    word_size = round(mark_size * 0.46)
    tag_size = round(mark_size * 0.15)
    tracking = round(mark_size * 0.03)

    word_length = text_width(WORDMARK, ADVANCE_BOLD, word_size, tracking)
    tag_lengths = [
        text_width(line, ADVANCE_REGULAR, tag_size) for line in TAGLINE
    ]
    column = max([word_length] + tag_lengths)
    width = round(text_x + column + pad)

    # The type is set as one block, optically centred against the mark: the
    # block runs from the top of the capitals to the last baseline, because
    # that is what the eye reads as its edges, not the ascenders and tails.
    word_cap = word_size * CAP_HEIGHT
    line_step = round(tag_size * 1.22)
    word_to_tag = round(tag_size * 1.45)
    block = word_cap + word_to_tag + line_step
    word_y = round((height - block) / 2 + word_cap)
    tag_ys = [word_y + word_to_tag, word_y + word_to_tag + line_step]

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
        f'aria-label="eMRTD, {TAGLINE[0]} {TAGLINE[1]}">',
        "  <title>eMRTD</title>",
        "  <style>",
        f"    svg:root {{ color: {INK_LIGHT} }}",
        "    @media (prefers-color-scheme: dark) {",
        f"      svg:root {{ color: {INK_DARK} }}",
        "    }",
        "  </style>",
        f'  <path fill="currentColor" fill-rule="evenodd" '
        f'd="{mark_outline(unit)}"/>',
    ]
    lines.append(
        f'  <text x="{text_x}" y="{word_y}" fill="currentColor" '
        f'font-family="{FONT_STACK}" font-size="{word_size}" '
        f'font-weight="600" textLength="{word_length:.0f}" '
        f'lengthAdjust="spacing">{WORDMARK}</text>'
    )
    for line, y, length in zip(TAGLINE, tag_ys, tag_lengths):
        lines.append(
            f'  <text x="{text_x}" y="{y}" fill="currentColor" '
            f'font-family="{FONT_STACK}" font-size="{tag_size}" '
            f'opacity="0.72" textLength="{length:.0f}" '
            f'lengthAdjust="spacing">{line}</text>'
        )
    lines.append("</svg>")

    # The frame has to hold what was drawn in it. This is the assertion the
    # drawing lacked when the wordmark was cropped, and it is cheap.
    for label, length in [(WORDMARK, word_length)] + list(zip(TAGLINE, tag_lengths)):
        assert text_x + length <= width - pad + 0.5, (
            f"{path}: {label!r} sets {length:.0f} units wide and runs past "
            f"the frame at {width - pad - text_x:.0f}"
        )
    assert tag_ys[-1] + tag_size * DESCENDER <= height, (
        f"{path}: the tagline drops below the frame"
    )

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"  {path.relative_to(ROOT)}  one path and three lines, {width}x{height}")


def write_raster_logo(path, scale=40):
    """A raster export of the mark, on a transparent background."""
    size = GRID * scale
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    for x, y, w, h in merged_rectangles():
        draw.rectangle(
            [x * scale, y * scale, (x + w) * scale - 1, (y + h) * scale - 1],
            fill=(0, 0, 0, 255),
        )
    image.save(path)
    print(f"  {path.relative_to(ROOT)}  {size}x{size}, RGBA")


def write_ascii(path):
    """The mark as text, in one block form and one plain form."""
    cells = mark_pixels()
    rows = range(PLATE[1], PLATE[3] + 1)
    cols = range(PLATE[0], PLATE[2] + 1)

    def render(on, off):
        return [
            "".join(on if (x, y) in cells else off for x in cols) for y in rows
        ]

    block = render("██", "  ")
    plain = render("##", "  ")

    wordmark = ["", "", "  e M R T D", "", f"  {TAGLINE[0]}", f"  {TAGLINE[1]}", "", ""]

    text = [
        "eMRTD",
        "=====",
        "",
        "The mark is the contact plate of the chip module inside a travel",
        "document: a plate divided into six pads. It is drawn on a ten by ten",
        "grid so that it survives the Flipper's application icon, and every",
        "larger form is that same grid scaled by a whole number.",
        "",
        "Banner:",
        "",
    ]
    for line, word in zip(block, wordmark):
        text.append(f"    {line}{word}")
    text += [
        "",
        "Plain 7-bit form, for a terminal that has no block characters:",
        "",
    ]
    text += [f"    {line}" for line in plain]
    text += [
        "",
        "Everything here is drawn by assets/make_logo.py, which also writes",
        "the application icon and the image the reading screen uses.",
        "",
    ]
    path.write_text("\n".join(text), encoding="utf-8")
    print(f"  {path.relative_to(ROOT)}  {len(block)} rows")


def main():
    print("Drawing the eMRTD mark:")
    # The launcher icon. One grid cell to one pixel - the hardest case, and
    # the one the whole design is sized for.
    write_bitmap(ROOT / "images" / "emrtd_10px.png", scale=1)
    # The reading screen. Two pixels per cell inside a two pixel margin keeps
    # the edges whole and the mark centred in twenty-four.
    write_bitmap(ROOT / "images" / "EmrtdChip_24x24.png", scale=2, margin=2)
    write_svg(ROOT / "assets" / "logo.svg")
    write_raster_logo(ROOT / "assets" / "logo.png")
    write_ascii(ROOT / "assets" / "logo.txt")


if __name__ == "__main__":
    main()
