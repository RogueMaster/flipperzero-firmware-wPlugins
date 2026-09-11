#!/usr/bin/env python3
"""Compare test/stubs against the real Flipper headers.

The stubs exist so the logic can be tested on a host, which only works
while they say the same thing the device's headers say. A stub that has
drifted compiles perfectly and then fails under ufbt, or worse, compiles
under ufbt and behaves differently - storage_common_mkdir returning
FS_Error rather than bool inverts every truthiness test written against
it, and FSE_OK is zero.

So this fetches the headers and diffs the signatures. Parameter names are
ignored; return types, parameter types and attributes are not.
"""
import os
import re
import sys
import urllib.request

# paths are relative to the repo, not to wherever this was run from
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BASE = ("https://raw.githubusercontent.com/flipperdevices/"
        "flipperzero-firmware/dev/")

UPSTREAM = [
    "applications/services/storage/storage.h",
    "applications/services/gui/canvas.h",
    "applications/services/gui/gui.h",
    "applications/services/gui/view_port.h",
    "applications/services/notification/notification_messages.h",
    "applications/services/notification/notification.h",
    "furi/core/message_queue.h",
    "furi/core/mutex.h",
    "furi/core/kernel.h",
    "furi/core/record.h",
    "targets/furi_hal_include/furi_hal_speaker.h",
    "targets/furi_hal_include/furi_hal_random.h",
    "targets/f7/furi_hal/furi_hal_rtc.h",
]

STUBS = [
    "test/stubs/furi.h",
    "test/stubs/furi_hal_speaker.h",
    "test/stubs/furi_hal_random.h",
    "test/stubs/furi_hal_rtc.h",
    "test/stubs/gui/gui.h",
    "test/stubs/storage/storage.h",
    "test/stubs/notification/notification_messages.h",
]

ATTRS = ("FURI_WARN_UNUSED", "FURI_RETURNS_NONNULL", "FURI_NORETURN",
         "FURI_DEPRECATED", "FURI_ALWAYS_INLINE")

KEYWORDS = {"void", "char", "short", "int", "long", "float", "double",
            "signed", "unsigned", "bool", "const", "struct", "enum",
            "union", "volatile", "restrict"}

DECL = re.compile(
    r"(?P<ret>[A-Za-z_][A-Za-z0-9_ \t\*]*?)\s*"
    r"\b(?P<name>[a-z_][A-Za-z0-9_]*)\s*"
    r"\((?P<args>[^()]*)\)\s*;",
    re.S)


def strip_noise(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    # a declaration never spans a preprocessor line in these headers
    text = re.sub(r"^\s*#.*$", " ", text, flags=re.M)
    return text


def norm_type(tok):
    tok = re.sub(r"\s*\*\s*", " * ", tok)
    return " ".join(tok.split())


def norm_param(p):
    p = p.strip()
    if not p:
        return ""
    p = re.sub(r"\[\s*\]", " *", p)          # arrays decay
    toks = re.sub(r"\s*\*\s*", " * ", p).split()
    if len(toks) > 1 and re.fullmatch(r"[A-Za-z_]\w*", toks[-1]) \
            and toks[-1] not in KEYWORDS and not toks[-1].endswith("_t"):
        toks = toks[:-1]                      # that was the parameter's name
    return " ".join(toks)


def parse(text):
    out = {}
    text = strip_noise(text)
    for m in DECL.finditer(text):
        ret = m.group("ret").strip()
        attrs = [a for a in ATTRS if a in ret]
        for a in ATTRS:
            ret = ret.replace(a, "")
        ret = norm_type(ret)
        if not ret or ret.split()[0] in ("return", "typedef", "else"):
            continue
        args = [norm_param(a) for a in m.group("args").split(",")]
        args = [a for a in args if a]
        out[m.group("name")] = (ret, tuple(args), tuple(sorted(attrs)))
    return out


def fetch(path):
    with urllib.request.urlopen(BASE + path, timeout=60) as r:
        return r.read().decode("utf-8", "replace")


def show(sig):
    ret, args, attrs = sig
    lead = ("".join(a + " " for a in attrs))
    return f"{lead}{ret} ({', '.join(args) or 'void'})"


def main():
    real = {}
    for path in UPSTREAM:
        try:
            real.update(parse(fetch(path)))
        except Exception as exc:                       # noqa: BLE001
            print(f"could not fetch {path}: {exc}", file=sys.stderr)
            return 2

    mine = {}
    missing = []
    for path in STUBS:
        full = os.path.join(ROOT, path)
        try:
            with open(full) as fh:
                mine.update(parse(fh.read()))
        except FileNotFoundError:
            missing.append(path)
    if missing:
        print("could not read: " + ", ".join(missing), file=sys.stderr)
        return 2

    checked = bad = unknown = 0
    for name in sorted(mine):
        if name not in real:
            unknown += 1
            continue
        checked += 1
        if mine[name] != real[name]:
            bad += 1
            print(f"MISMATCH {name}")
            print(f"    real {show(real[name])}")
            print(f"    stub {show(mine[name])}")

    print(f"\n{checked} shared signatures checked, {bad} mismatched, "
          f"{unknown} stub declarations not found upstream")
    if not checked:
        # a check that silently verifies nothing is worse than no check
        print("\nnothing was actually compared", file=sys.stderr)
        return 1
    if bad:
        print("\nthe stubs have drifted from the device headers")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
