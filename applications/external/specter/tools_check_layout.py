#!/usr/bin/env python3
"""Static layout checker for the Flipper views.

Two collisions reached real users - the FIELD number sitting on top of the PK
row, and the divider line slicing through the NOISE FLOOR text - because
nothing checked vertical extents. Eyeballing a 128x64 screen does not scale,
and the mockup renderer uses a desktop font whose metrics are close but not
identical, so it drew both of those as "fine".

This reads the view sources directly and reports every drawing primitive whose
vertical band overlaps another's. Font bands are derived from the two confirmed
device collisions above:

    FontSecondary / FontPrimary   rows [baseline-7 .. baseline]      (8 tall)
    FontBigNumbers                rows [baseline-18 .. baseline]     (19 tall)

It cannot know which branch of an if/else actually runs, so it reports
CANDIDATES for a human to adjudicate. Anything drawn in mutually exclusive
branches is expected to "collide" here and is fine.

SECOND PASS: CLEARANCE. Overlap is not the only way a screen goes wrong. In
v3.0 the Site Survey progress bar was widened to a 14-row frame, putting its
bottom edge on row 27 with the "FIELD 0%" capitals starting on row 28 - not
overlapping, so this script said nothing, but with zero white rows between them
the text visibly fused with the bar on real hardware. The owner found it in a
screenshot; the tool should have.

So there is now a clearance pass, and it uses a tighter, measured ink model
rather than the conservative band above. Taken off 4x device captures:

    FontSecondary   ink rows [baseline-7 .. baseline-1]   (the baseline row
                    itself is blank for glyphs without descenders)
    FontPrimary     ink rows [baseline-8 .. baseline-1]
    lowercase g j p q y descend about two rows further

That distinction matters: by the conservative band Fingerprint's stat rows
looked like they touched, when on the device they had one blank row - tight,
but not fused. Guessing either way is how both of these shipped, so the numbers
come from pixels now.

    python3 tools_check_layout.py
"""
import re, sys, os

FONT_ASCENT = {
    "FontSecondary": 7,
    "FontPrimary": 7,
    "FontKeyboard": 7,
    "FontBigNumbers": 18,
}

# Measured ink heights (see the clearance pass in the docstring). The band above
# is deliberately conservative for OVERLAP; these are what the glyphs actually
# light, and are what clearance is judged on.
INK_ASCENT = {
    "FontSecondary": 7,
    "FontPrimary": 8,
    "FontKeyboard": 7,
    "FontBigNumbers": 18,
}

# Blank rows the house style keeps between a text block and anything else. Every
# stacked pair in the app that reads well has two; the ones that shipped looking
# wrong had zero or one.
MIN_GAP = 2

VIEWS = "views"

str_re = re.compile(r"canvas_draw_str\s*\(\s*canvas\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*,")
stra_re = re.compile(
    r"canvas_draw_str_aligned\s*\(\s*canvas\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*,"
)
line_re = re.compile(
    r"canvas_draw_line\s*\(\s*canvas\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*\)"
)
box_re = re.compile(
    r"canvas_draw_(box|frame)\s*\(\s*canvas\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*,\s*(-?\w+)\s*\)"
)
font_re = re.compile(r"canvas_set_font\s*\(\s*canvas\s*,\s*(\w+)\s*\)")


def resolve(tok, defines):
    if tok is None:
        return None
    if re.fullmatch(r"-?\d+", tok):
        return int(tok)
    return defines.get(tok)


def scan(path):
    src = open(path).read()
    defines = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(-?\d+)\s*(?://.*)?$", src, re.M):
        defines[m.group(1)] = int(m.group(2))

    items = []
    font = "FontSecondary"
    for n, raw in enumerate(src.splitlines(), 1):
        fm = font_re.search(raw)
        if fm:
            font = fm.group(1)
            continue
        for rx in (str_re, stra_re):
            m = rx.search(raw)
            if m:
                y = resolve(m.group(2), defines)
                if y is None:
                    continue
                asc = FONT_ASCENT.get(font, 7)
                items.append(
                    (n, "text", font, y - asc, y, raw.strip()[:58], m.group(1))
                )
                break
        m = line_re.search(raw)
        if m:
            y0, y1 = resolve(m.group(2), defines), resolve(m.group(4), defines)
            if y0 is not None and y0 == y1:
                items.append((n, "hline", "", y0, y0, raw.strip()[:58], None))
        m = box_re.search(raw)
        if m:
            y, h = resolve(m.group(3), defines), resolve(m.group(5), defines)
            if y is not None and h is not None:
                if m.group(1) == "frame":
                    # An outline occupies only its top and bottom rows, not the
                    # span between them - treating it as a solid band was
                    # reporting every alarm border as colliding with everything.
                    items.append((n, "frame-top", "", y, y, raw.strip()[:58], None))
                    items.append(
                        (
                            n,
                            "frame-bot",
                            "",
                            y + h - 1,
                            y + h - 1,
                            raw.strip()[:58],
                            None,
                        )
                    )
                else:
                    items.append((n, "box", "", y, y + h - 1, raw.strip()[:58], None))
    return items


def overlaps(a, b):
    return a[3] <= b[4] and b[3] <= a[4]


problems = 0
for fn in sorted(os.listdir(VIEWS)):
    if not fn.endswith(".c"):
        continue
    path = os.path.join(VIEWS, fn)
    items = scan(path)
    hits = []
    for i, a in enumerate(items):
        if a[1] != "text":
            continue
        for j, b in enumerate(items):
            if i == j:
                continue
            # Compare against everything, not just later draws - the line that
            # slices through a label is usually drawn BEFORE it.
            if b[1] == "box":
                continue  # inverted text over a filled box is intentional
            if b[1] == "text":
                # same row band in different columns is normal; only flag when
                # both sit at the same x, where overlap is certain
                if j < i or a[6] != b[6]:
                    continue
            if overlaps(a, b):
                hits.append((a, b))
    if hits:
        print(f"\n=== {path} ===")
        for a, b in hits:
            print(f"  L{a[0]:<4} {a[2]:<14} rows {a[3]:>3}..{a[4]:<3} | {a[5]}")
            print(f"  L{b[0]:<4} {b[1]:<14} rows {b[3]:>3}..{b[4]:<3} | {b[5]}")
            print()
            problems += 1

print(
    f"\n{problems} candidate collision(s) - check each against the if/else structure."
)


# --------------------------------------------------------------------------
# clearance: text that does not overlap its neighbour but sits right on it
# --------------------------------------------------------------------------
def ink(item):
    """Rows this item actually lights, on the measured model."""
    n, kind, font, top, bot, raw, x = item
    if kind != "text":
        return top, bot
    asc = INK_ASCENT.get(font, 7)
    return bot - asc, bot - 1  # bot is the baseline; that row is blank


# Two failure modes, two rules, because one threshold fits neither:
#
#   * text hard against a bar, box edge or divider. ZERO blank rows is the
#     defect (Site Survey v3.0). One row is not - the alarm border runs along
#     rows 0 and 63 and every header and footer sits one row inside it, which is
#     deliberate and looks right.
#   * two text rows stacked in the same column. Here the house pitch is 10px,
#     i.e. TWO blank rows, and every screen that reads well uses it; Fingerprint
#     briefly used 8px and looked visibly denser than the rest of the app.
tight = 0
for fn in sorted(os.listdir(VIEWS)):
    if not fn.endswith(".c"):
        continue
    path = os.path.join(VIEWS, fn)
    items = scan(path)
    hits = []
    for i, a in enumerate(items):
        if a[1] != "text":
            continue
        at, ab = ink(a)
        for j, b in enumerate(items):
            if i == j:
                continue
            bt, bb = ink(b)
            if at <= bb and bt <= ab:
                continue  # overlapping: the first pass owns that

            stacked_text = b[1] == "text"
            if stacked_text:
                # only meaningful in the same column, and only count each pair once
                if j < i or a[6] != b[6]:
                    continue
                want = MIN_GAP
            else:
                # inverted text inside a filled box is the alarm-bar idiom
                if b[1] == "box" and bt <= at and ab <= bb:
                    continue
                want = 1  # zero blank rows is the defect; one is the house edge

            gap = (at - bb - 1) if at > bb else (bt - ab - 1)
            if gap < want:
                hits.append((a, b, gap, want))
    if hits:
        print(f"\n=== {path} (clearance) ===")
        for a, b, gap, want in hits:
            at, ab = ink(a)
            bt, bb = ink(b)
            print(
                f"  {'TOUCHING' if gap == 0 else 'tight'}: {gap} blank row(s), wanted {want}"
            )
            print(f"  L{a[0]:<4} {a[2]:<14} ink {at:>3}..{ab:<3} | {a[5]}")
            print(f"  L{b[0]:<4} {b[1]:<14} ink {bt:>3}..{bb:<3} | {b[5]}")
            print()
            tight += 1

print(f"{tight} tight-clearance candidate(s) - fewer than {MIN_GAP} blank rows.")

# Most candidates are legitimate: text drawn in mutually exclusive branches, or
# sharing a row in different columns. Rather than teach this script to follow
# control flow, CI pins the count - any NEW collision pushes it over the line
# and has to be looked at.
failed = False
if "--max" in sys.argv:
    limit = int(sys.argv[sys.argv.index("--max") + 1])
    if problems > limit:
        print(
            f"FAIL: {problems} overlap candidates, baseline is {limit}. Adjudicate the new one(s)."
        )
        failed = True
    else:
        print(f"OK: within the {limit}-candidate overlap baseline.")

if "--max-tight" in sys.argv:
    limit = int(sys.argv[sys.argv.index("--max-tight") + 1])
    if tight > limit:
        print(
            f"FAIL: {tight} tight-clearance candidates, baseline is {limit}. Adjudicate the new one(s)."
        )
        failed = True
    else:
        print(f"OK: within the {limit}-candidate clearance baseline.")

if failed:
    sys.exit(1)
