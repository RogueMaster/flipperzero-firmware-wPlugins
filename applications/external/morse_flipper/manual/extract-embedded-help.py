#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import hashlib
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
help_c = root / "src" / "firmware" / "morse_flipper_help.c"
out_md = root / "manual" / "internal-help.md"
esc = "\x1b"
skip_md5 = "622ada3310f55b86677b2ac6a346ba81"

chapters = [
    ("First steps", "morse_help_first_steps"),
    ("Input & keys", "morse_help_input_keys"),
    ("Connecting the paddle", "morse_help_connecting_paddle"),
    ("How to practice", "morse_help_practice"),
    ("Prepping", "morse_help_prepping"),
    ("A complete Morse contact", "morse_help_contact"),
    ("Contesting", "morse_help_contesting"),
    ("USB & live practice", "morse_help_usb_live"),
    ("Ham usage", "morse_help_ham_usage"),
    ("Troubleshooting", "morse_help_troubleshooting"),
    ("Moving forward", "morse_help_moving_forward"),
]

glyph = {
    1: "→",
    2: "µ",
    3: "⏴",
    4: "•",
    5: "—",
    6: "/",
}


def cstr(s):
    s = s[1:-1]
    r = ""
    i = 0
    while i < len(s):
        if s[i] != "\\":
            r += s[i]
            i += 1
            continue
        i += 1
        if i >= len(s):
            r += "\\"
            break
        c = s[i]
        if c == "n":
            r += "\n"
        elif c == "r":
            r += "\r"
        elif c == "t":
            r += "\t"
        elif c == "e":
            r += esc
        elif c in ['"', "'", "\\"]:
            r += c
        elif c == "x":
            h = s[i + 1 : i + 3]
            if re.match(r"^[0-9a-fA-F]{2}$", h):
                r += chr(int(h, 16))
                i += 2
            else:
                r += c
        elif c in "01234567":
            j = i
            while j < len(s) and j < i + 3 and s[j] in "01234567":
                j += 1
            r += chr(int(s[i:j], 8))
            i = j - 1
        else:
            r += c
        i += 1
    return r


def cards(name, src):
    m = re.search(r"static const char\* const " + re.escape(name) + r"\[\] = \{(.*?)\};", src, re.S)
    if not m:
        raise SystemExit("missing array")
    return [cstr(x) for x in re.findall(r'"(?:\\.|[^"\\])*"', m.group(1))]


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
    s = iconize(s)
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


src = help_c.read_text()
md = [
    "# Internal Help",
    "",
    "This is the same help file that lives on the Flipper under Help: a gentle introduction to Morse code, telegraphy, or CW. It points at how to learn, _what_ is worth practising, which resources are worth seeking out, and where to go next once the first noises start becoming letters.",
    "",
]
for title, name in chapters:
    md += ["## " + title, ""]
    for card in cards(name, src):
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
