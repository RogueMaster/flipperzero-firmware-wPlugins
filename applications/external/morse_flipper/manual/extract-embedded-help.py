#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import hashlib
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
help_dir = root / "assets" / "help"
out_md = root / "manual" / "internal-help.md"
esc = "\x1b"
skip_md5 = "622ada3310f55b86677b2ac6a346ba81"

chapters = [
    ("First steps", "01-first-steps"),
    ("Input & keys", "02-input-and-keys"),
    ("Connecting the paddle", "03-connecting-the-paddle"),
    ("How to practice", "04-how-to-practice"),
    ("Prepping", "05-prepping"),
    ("A complete Morse contact", "06-a-complete-morse-contact"),
    ("Contesting", "07-contesting"),
    ("USB & live practice", "08-usb-and-live-practice"),
    ("Ham usage", "09-ham-usage"),
    ("Troubleshooting", "10-troubleshooting"),
    ("Moving forward", "11-moving-forward"),
]

glyph = {
    1: "→",
    2: "µ",
    3: "⏴",
    4: "•",
    5: "—",
    6: "/",
}


def cards(name):
    p = help_dir / name
    if not p.exists():
        raise SystemExit("missing " + name)
    return [x.strip("\r\n") for x in re.split(r"(?m)^---$", p.read_text())]


def iconize(s):
    r = ""
    i = 0
    while i < len(s):
        if s[i] != esc:
            r += s[i]
            i += 1
            continue
        i += 1
        if i >= len(s):
            break
        if s[i] == "x":
            i += 1
            j = i
            while j < len(s) and j < i + 2 and s[j].isdigit():
                j += 1
            if j > i:
                r += glyph.get(int(s[i:j]), "")
                i = j
                continue
        if s[i] in "lcr#*":
            i += 1
            continue
        i += 1
    return r


def strip_soft_breaks(s):
    r = ""
    i = 0
    while i < len(s):
        if s[i] == "~":
            i += 1
            if i < len(s) and s[i] == "~":
                r += "~"
                i += 1
            continue
        r += s[i]
        i += 1
    return r


def fmt_line(s):
    bold = False
    mono = False
    s = s.strip()
    while s.startswith(esc) and len(s) > 1:
        c = s[1]
        if c == "#":
            bold = True
        elif c == "*":
            mono = True
        elif c not in "lcr":
            break
        s = s[2:].lstrip()
    s = strip_soft_breaks(iconize(s))
    s = re.sub(r"\s+", " ", s).strip()
    s = s.replace(r"\_", "_").replace(r"\`", "`")
    s = re.sub(r"__(.+?)__", r"**\1**", s)
    if mono and s:
        s = "`" + s.replace("`", "'") + "`"
    if bold and s:
        s = "**" + s + "**"
    return s


def show_card(s):
    for w in re.findall(r"[A-Za-z0-9_./@+-]+", s):
        if hashlib.md5(w.encode()).hexdigest() == skip_md5:
            return False
    return True


def render_card(s):
    prose = []
    subs = []
    for ln in s.replace("\r", "").split("\n"):
        t = fmt_line(ln)
        if not t:
            continue
        if t.startswith("- "):
            subs.append(t[2:].strip())
        else:
            prose.append(t)
    if not prose:
        prose.append(" ")
    r = ["- " + " ".join(prose)]
    for x in subs:
        r.append("  - " + x)
    return r


def must_link(s, old, new):
    if old not in s:
        print("warn: missing text: " + old, file=sys.stderr)
        return s
    return s.replace(old, new)


md = [
    "# Internal Help",
    "",
    "This is the same help file that lives on the Flipper under Help: a gentle introduction to Morse code, telegraphy, or CW. It points at how to learn, _what_ is worth practising, which resources are worth seeking out, and where to go next once the first noises start becoming letters.",
    "",
]
for title, name in chapters:
    md += ["## " + title, ""]
    for card in cards(name):
        if show_card(card):
            md += render_card(card)
    md.append("")

out_md.parent.mkdir(exist_ok=True)
txt = "\n".join(md).rstrip() + "\n"
txt = must_link(
    txt,
    "shave and a haircut",
    "[Shave and a Haircut](https://en.wikipedia.org/wiki/Shave_and_a_Haircut)",
)
txt = must_link(
    txt,
    "Tune around 14.000-14.070",
    "[Tune around 14.000-14.070](http://websdr.ewi.utwente.nl:8901/?tune=14044cw)",
)
out_md.write_text(txt)
