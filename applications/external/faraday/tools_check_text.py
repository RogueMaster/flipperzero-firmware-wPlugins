#!/usr/bin/env python3
"""Check that every on-screen string actually fits the Flipper's 128px screen.

Two whole classes of bug ship silently without this, and both of them reached
a real device in this repo before it existed:

  * A line of scrolling text wider than the view. The widget wraps it, but it
    wraps on width, not on words, so "2. SHIELDED - the same signal" came back
    as "2. SHIELDED - the same sign" / "al". Hand-wrapping the source to a
    character count is what fails: the font is PROPORTIONAL, so 30 characters
    of lowercase fit and 30 characters with capitals do not.

  * A closing "\\e#". The SDK documents \\e# as "sets bold font before until
    next '\\n'" - a line PREFIX, not a wrapper - so a trailing one is not a
    terminator, it is two literal characters and the '#' gets printed. Every
    heading in the app was rendering a stray '#'.

    python3 tools_check_text.py          # report, exit 1 on any overflow
    python3 tools_check_text.py --list   # print every string with its width

Widths are calibrated against a real device capture rather than guessed: in
screenshots/, FontSecondary renders "Prove your signal-blocking" (26 chars) at
about 110 px, i.e. ~4.2 px per lowercase character, and a 30-character line
overflowed the 124 px of usable width. The table below reproduces that.
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# The scrolling text element is the full 128px wide with a couple of pixels of
# margin and a scrollbar column on the right.
SCROLL_WIDTH = 120

# FontSecondary (haxrcorp_4089) is proportional. These are conservative
# per-character advances in pixels, calibrated so that the known-good and
# known-bad lines from the device captures land on the right side of the
# limit. Erring high is correct: a false alarm costs a rewrap, a missed one
# ships a clipped word.
NARROW = set("iljt.,:;'!|()[]/\\ ")
WIDE = set("mwMW@")


def text_width(s):
    """Estimated rendered width in pixels.

    Calibrated against the device, not invented. In a capture of the About
    screen "Prove your signal-blocking" (26 chars) fits inside the view while
    "Every test is two captures and" (30 chars) runs off the right edge with
    its last letter sliced - so the true advance is just over 4.2 px for a
    lowercase character. An earlier version of this table ran about 12% light
    and passed that second line as fitting, which is exactly the failure this
    file exists to prevent, so the numbers below are the corrected ones.
    """
    w = 0
    for ch in s:
        if ch in NARROW:
            w += 2.9
        elif ch in WIDE:
            w += 7.2
        elif ch.isupper() or ch.isdigit():
            w += 5.8
        else:
            w += 4.9
    return w


# A C string literal, and the escape-prefixed lines inside it.
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(s):
    return (
        s.replace("\\e", "\x1b")
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\\\\", "\\")
    )


# Literals handed to these never reach a screen: they are the on-disk CSV
# format and its matching parser. Measuring them produces noise that trains
# you to ignore the report, which is worse than not having one.
OFF_SCREEN = ("snprintf", "sscanf", "storage_file_", "file_stream_")


# Not calls for this purpose - they appear inside an argument list and would
# otherwise mask the function that actually owns the literal.
NOT_A_CALL = {"sizeof", "if", "while", "for", "switch", "return"}


def enclosing_call(src, pos):
    """The function whose argument list this literal sits in.

    Walks back over balanced parentheses so a nested call in an earlier
    argument - sscanf(furi_string_get_cstr(line), "...") - does not get
    mistaken for the function that owns the literal.
    """
    depth, i = 0, pos - 1
    while i >= 0 and pos - i < 2000:
        ch = src[i]
        if ch == ")":
            depth += 1
        elif ch == "(":
            if depth == 0:
                name = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", src[:i])
                if name and name.group(1) not in NOT_A_CALL:
                    return name.group(1)
                return ""
            depth -= 1
        i -= 1
    return ""


# A format string whose %s arguments the tool cannot see the size of can
# declare its real worst case:
#
#     furi_string_cat_printf(out, "\e#%s  %d dB+\n", ...);  // screen-ok: "A+  60 dB+"
#
# The declared sample is measured instead of the guessed one. This is on
# purpose rather than a suppression list: it puts a concrete, checkable string
# in the source next to the format, so it stays honest when the format changes.
OK_HINT = re.compile(r'//\s*screen-ok:\s*"((?:[^"\\]|\\.)*)"')


def strip_comments(src):
    """Blank out C comments, preserving line numbering.

    Without this, a quotation mark inside a COMMENT reads as the start of a
    string literal and the checker measures prose that is never drawn - which
    it did, failing the build over a sentence in a code comment. Newlines are
    kept so reported line numbers still point at the right place.
    """
    out, i, n = [], 0, len(src)
    while i < n:
        two = src[i : i + 2]
        if two == "/*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(c if c == "\n" else " " for c in src[i:j]))
            i = j
        elif two == "//":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif src[i] == '"':  # step over a real literal so a // inside it is safe
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(src[i:j])
            i = j
        else:
            out.append(src[i])
            i += 1
    return "".join(out)


def scan(path):
    """Yield (line_no, rendered_line, width, note) for every screen line."""
    raw = open(path).read()
    src = strip_comments(raw)
    lines_of = raw.splitlines()  # hints live in the ORIGINAL text
    out = []
    for m in LIT.finditer(src):
        lit = unescape(m.group(1))
        if "\n" not in lit and "\x1b" not in lit:
            continue  # not a multi-line screen string
        call = enclosing_call(src, m.start())
        if any(call.startswith(p) for p in OFF_SCREEN):
            continue
        line_no = src[: m.start()].count("\n") + 1
        # A hint may sit on the literal's line or on the line that closes the
        # call, so look at the small window around it.
        hint = None
        for probe in range(line_no - 1, min(line_no + 3, len(lines_of))):
            h = OK_HINT.search(lines_of[probe])
            if h:
                hint = unescape(h.group(1))
                break
        if hint is not None:
            # The sample stands in for the WHOLE rendered block, so every line
            # of it is measured - otherwise annotating a multi-line format
            # would quietly stop checking its other lines.
            for hl in hint.split("\n"):
                if not hl.strip():
                    continue
                w = text_width(hl)
                note = "" if w <= SCROLL_WIDTH else "declared sample overflows"
                out.append((line_no, hl, hl, w, note))
            continue
        for raw in lit.split("\n"):
            note = ""
            body = raw
            # A leading \e<x> is a formatting prefix and costs no pixels.
            if body.startswith("\x1b") and len(body) > 1:
                body = body[2:]
            # Any REMAINING escape is the closing-\e# bug: \e is dropped and
            # the character after it is printed.
            stray = body.count("\x1b")
            if stray:
                note = f"stray \\e (prints {stray} literal char(s))"
                body = body.replace("\x1b", "")
            # printf conversions stand in for their widest plausible output:
            # a frequency in Hz is 9 digits, a dB/% figure is at most 4
            # characters with its sign, and every %s here is a short label
            # (a grade letter, a band name) rather than free text.
            # An explicit field width is the real width: %02u is two digits,
            # never four. Without this the timestamps in the saved-results
            # entry measured almost twice their actual size and the report
            # cried wolf on a line that fits comfortably.
            body = re.sub(
                r"%0?([1-9])[0-9]*l*[du]", lambda m: "9" * int(m.group(1)), body
            )
            body = re.sub(r"%[-+ #.]*lu", "999999999", body)
            body = re.sub(r"%[-+ #.]*l*[du]", "-100", body)
            # Every %s on a screen here is a short label: a grade ("A+"), a
            # band ("13.56 MHz"). Nine characters covers the longest of them.
            body = re.sub(r"%[-+ #0-9.]*s", "XXXXXXXXX", body)
            body = body.replace("%%", "%")
            if not body.strip() and not note:
                continue
            out.append((line_no, raw, body, text_width(body), note))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--list", action="store_true", help="print every line, not just problems"
    )
    args = ap.parse_args()

    files = sorted(
        glob.glob(os.path.join(HERE, "scenes", "*.c"))
        + glob.glob(os.path.join(HERE, "helpers", "*.c"))
    )
    bad = 0
    for path in files:
        rows = scan(path)
        if not rows:
            continue
        shown = False
        for line_no, raw, body, w, note in rows:
            over = w > SCROLL_WIDTH
            if not (over or note or args.list):
                continue
            if not shown:
                print(f"\n{os.path.relpath(path, HERE)}")
                shown = True
            flag = "OVER" if over else ("BUG " if note else "ok  ")
            if over or note:
                bad += 1
            extra = f"  <- {note}" if note else ""
            print(f"  {flag} line {line_no:>4}  {w:5.0f}px  {body!r}{extra}")

    print()
    if bad:
        print(
            f"{bad} line(s) do not fit or contain a stray escape (limit {SCROLL_WIDTH}px)"
        )
        return 1
    print(f"all screen strings fit within {SCROLL_WIDTH}px")
    return 0


if __name__ == "__main__":
    sys.exit(main())
