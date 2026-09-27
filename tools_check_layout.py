#!/usr/bin/env python3
"""Check that every string GhostTag draws actually fits its 128x64 screen.

Three classes of bug ship silently without this, and all three have reached a
real device somewhere in this family of apps:

  * A string wider than the space it is drawn into. The firmware does not wrap
    and does not complain - it just runs off the right edge mid-word. Hand
    counting characters does not work, because the fonts are PROPORTIONAL: 26
    lowercase characters fit where 26 with capitals do not.

  * A baseline too low. FontSecondary and FontPrimary occupy rows
    [baseline-7 .. baseline], but descenders - g y p q j - drop about two rows
    BELOW that. Text on baseline 62 has its tails sliced off by the bottom of
    the screen, and text on 63 loses a row of the glyph itself.

  * A closing "\\e#". The SDK documents \\e# as "sets bold font until the next
    newline" - a line PREFIX, not a wrapper - so a trailing one is not a
    terminator. It is two literal characters, and the '#' gets printed.

    python3 tools_check_layout.py          # report, exit 1 on any problem
    python3 tools_check_layout.py --list   # print every string with its width

Widths are calibrated against real device captures rather than guessed. Erring
high is correct: a false alarm costs a rewrap, a missed one ships a clipped
word to everybody who installs the app.
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

SCREEN_W = 128
SCREEN_H = 64

# The scrolling text element is the full width less a margin and the scrollbar.
SCROLL_WIDTH = 120

# Lowest safe baseline for a font with descenders on a 64-row screen.
MAX_BASELINE = 61

NARROW = set("iljt.,:;'!|()[]/\\ ")
WIDE = set("mwMW@")

# FontPrimary is the bolder, wider face; FontSecondary is haxrcorp_4089.
FONT_SCALE = {"FontPrimary": 1.20, "FontSecondary": 1.0, "FontKeyboard": 1.0}
# FontBigNumbers is a fixed-width digit face, measured off a capture.
BIGNUM_ADVANCE = 12.0


def text_width(s, font="FontSecondary"):
    if font == "FontBigNumbers":
        return len(s) * BIGNUM_ADVANCE
    w = 0.0
    for ch in s:
        if ch in NARROW:
            w += 2.9
        elif ch in WIDE:
            w += 7.2
        elif ch.isupper() or ch.isdigit():
            w += 5.8
        else:
            w += 4.9
    return w * FONT_SCALE.get(font, 1.0)


LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')

DRAW_STR = re.compile(
    r"canvas_draw_str\s*\(\s*canvas\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*"
)
DRAW_ALIGNED = re.compile(
    r"canvas_draw_str_aligned\s*\(\s*canvas\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*"
    r"(Align\w+)\s*,\s*(Align\w+)\s*,\s*"
)
SET_FONT = re.compile(r"canvas_set_font\s*\(\s*canvas\s*,\s*(Font\w+)\s*\)")
SCROLL = re.compile(r"widget_add_text_scroll_element\s*\(", re.S)


def unescape(s):
    return (
        s.replace("\\e", "\x1b")
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\\\\", "\\")
    )


def literals_in_call(src, pos):
    """Every string literal in the rest of this call's argument list.

    Matching only a literal that starts immediately at `pos` misses the ones
    inside a ternary - canvas_draw_str(canvas, 48, 48, demo ? "A" : "B") - and
    that blind spot shipped: "SIMULATED - not real" was drawn eleven pixels off
    the right-hand edge of a real device and this checker reported the screen
    as clean. Both branches of a conditional are strings that can be on screen,
    so both get measured.
    """
    depth = 1
    i = pos
    out = []
    n = len(src)
    while i < n and depth > 0:
        ch = src[i]
        if ch == '"':
            m = LIT.match(src, i)
            if not m:
                break
            out.append(unescape(m.group(1)))
            i = m.end()
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                break
        elif ch == ";":
            break
        i += 1
    return out


def check_file(path, listing, problems):
    src = open(path).read()
    rel = os.path.relpath(path, HERE)

    def lineno(pos):
        return src.count("\n", 0, pos) + 1

    # Track the most recent font set before each draw call.
    fonts = [(m.start(), m.group(1)) for m in SET_FONT.finditer(src)]

    def font_at(pos):
        cur = "FontSecondary"
        for at, f in fonts:
            if at < pos:
                cur = f
            else:
                break
        return cur

    for m in DRAW_STR.finditer(src):
        x, baseline = int(m.group(1)), int(m.group(2))
        font = font_at(m.start())
        for s in literals_in_call(src, m.end()):
            w = text_width(s, font)
            right = x + w
            listing.append((rel, lineno(m.start()), font, round(w), f"x={x} y={baseline}", s))
            if right > SCREEN_W:
                problems.append(
                    f"{rel}:{lineno(m.start())}  runs off the right edge: "
                    f'"{s}" at x={x} is ~{w:.0f}px wide, ending at {right:.0f} (>{SCREEN_W})'
                )
            if baseline > MAX_BASELINE:
                problems.append(
                    f"{rel}:{lineno(m.start())}  baseline {baseline} clips descenders "
                    f'(max {MAX_BASELINE}): "{s}"'
                )

    for m in DRAW_ALIGNED.finditer(src):
        x, baseline = int(m.group(1)), int(m.group(2))
        halign = m.group(3)
        font = font_at(m.start())
        for s in literals_in_call(src, m.end()):
            w = text_width(s, font)
            if halign == "AlignRight":
                left, right = x - w, x
            elif halign == "AlignCenter":
                left, right = x - w / 2, x + w / 2
            else:
                left, right = x, x + w
            listing.append(
                (rel, lineno(m.start()), font, round(w), f"{halign} x={x} y={baseline}", s)
            )
            if right > SCREEN_W or left < 0:
                problems.append(
                    f"{rel}:{lineno(m.start())}  {halign} string leaves the screen: "
                    f'"{s}" spans {left:.0f}..{right:.0f} (0..{SCREEN_W})'
                )
            if baseline > MAX_BASELINE:
                problems.append(
                    f"{rel}:{lineno(m.start())}  baseline {baseline} clips descenders "
                    f'(max {MAX_BASELINE}): "{s}"'
                )

    # Scrolling text elements: every line measured against the usable width.
    for m in SCROLL.finditer(src):
        chunk = src[m.end() : m.end() + 6000]
        joined = "".join(LIT.findall(chunk))
        for line in unescape(joined).split("\n"):
            if not line.strip():
                continue
            w = text_width(line, "FontSecondary")
            listing.append((rel, lineno(m.start()), "scroll", round(w), "text_scroll", line))
            if w > SCROLL_WIDTH:
                problems.append(
                    f"{rel}:{lineno(m.start())}  scroll line wraps mid-word: "
                    f'"{line}" is ~{w:.0f}px (>{SCROLL_WIDTH})'
                )

    # A closing \e# is not a terminator; it prints a literal '#'.
    for m in LIT.finditer(src):
        s = unescape(m.group(1))
        for esc in ("\x1b#", "\x1bc", "\x1br"):
            body = s
            # An escape is only legal at the very start of a line.
            for i, ch in enumerate(body):
                if ch == "\x1b" and i > 0 and body[i - 1] != "\n":
                    problems.append(
                        f"{rel}:{lineno(m.start())}  \\e escape mid-line - it is a line "
                        f"PREFIX, not a wrapper, so this prints a literal character: "
                        f"{body[i:i+3]!r}"
                    )
                    break
            break


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true", help="print every string with its width")
    args = ap.parse_args()

    files = sorted(
        glob.glob(os.path.join(HERE, "views", "*.c"))
        + glob.glob(os.path.join(HERE, "scenes", "*.c"))
    )
    listing, problems = [], []
    for f in files:
        check_file(f, listing, problems)

    if args.list:
        for rel, ln, font, w, where, s in listing:
            print(f"{rel}:{ln:<4} {font:<15} {w:>4}px  {where:<26} {s!r}")
        print()

    print(f"checked {len(listing)} strings across {len(files)} files")
    if problems:
        print(f"\n{len(problems)} problem(s):\n")
        for p in problems:
            print("  " + p)
        return 1
    print("every string fits, every baseline clears the bottom row")
    return 0


if __name__ == "__main__":
    sys.exit(main())
