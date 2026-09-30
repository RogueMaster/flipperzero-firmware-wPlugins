"""WCAG AA check for the project site's colour tokens, both themes.

The house rule is that contrast is verified by a test, not by eye - it has
caught real failures the eye passed. Run it after touching any token.
"""

import re, sys

css = open(
    sys.argv[1]
    if len(sys.argv) > 1
    else __import__("os").path.join(
        __import__("os").path.dirname(__import__("os").path.abspath(__file__)),
        "docs",
        "index.html",
    )
).read()


def block(start_line_marker):
    i = css.index(start_line_marker)
    j = css.index("\n}", i)
    return dict(re.findall(r"(--[\w-]+)\s*:\s*(#[0-9A-Fa-f]{6})", css[i:j]))


light = block("\n:root{")
dark = block('\n:root[data-theme="dark"]{')


def lum(h):
    c = [int(h[i : i + 2], 16) / 255 for i in (1, 3, 5)]
    c = [v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4 for v in c]
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def ratio(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


PAIRS = [
    ("body text", "--ink", "--paper", 4.5),
    ("body on card", "--ink", "--card", 4.5),
    ("muted text", "--muted", "--paper", 4.5),
    ("muted on card", "--muted", "--card", 4.5),
    ("link", "--link", "--paper", 4.5),
    ("link on card", "--link", "--card", 4.5),
    ("eyebrow gold", "--gold", "--paper", 4.5),
    ("gold on card", "--gold", "--card", 4.5),
    ("measure number", "--gold", "--card", 3.0),  # 40px display = large text
    ("yes verdict", "--yes", "--card", 4.5),
    ("no verdict", "--no", "--card", 4.5),
]

fails = 0
for name, t in (("light", light), ("dark", dark)):
    print(f"--- {name} ({len(t)} tokens) ---")
    for label, fg, bg, need in PAIRS:
        if fg not in t or bg not in t:
            print(f"  MISSING {label}: {fg} / {bg}")
            fails += 1
            continue
        r = ratio(t[fg], t[bg])
        ok = r >= need
        fails += not ok
        print(
            f"  {'pass' if ok else 'FAIL'}  {label:15s} {t[fg]} on {t[bg]}  {r:5.2f}:1  (need {need})"
        )
print()
print("FAILURES:", fails)
sys.exit(1 if fails else 0)
