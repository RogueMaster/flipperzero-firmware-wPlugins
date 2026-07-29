#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
"""
Fail if the OUI tables in the Flipper app and the ESP32 companion have drifted.

WHY THIS EXISTS. The same two tables are compiled into two different binaries
built by two different toolchains:

    helpers/flock_db.c                                flock_ouis[]  / soundthinking_ouis[]
    esp32_companion/flock_companion/flock_companion.ino  FLOCK_OUIS[] / SOUNDTHINKING_OUIS[]

There is no shared header -- the companion is an Arduino sketch that cannot
include the app's headers -- so both files carry a comment saying "keep these in
step by hand". Nothing enforced it. Editing one side alone silently desyncs
ESP-side scoring from the Flipper's, and the failure is invisible: detections
just quietly differ depending on which side saw the frame first.

A comment is not a guard. This is.

Usage:  python tools/check_oui_parity.py     (exit 0 = in sync, 1 = drifted)
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
APP = ROOT / "helpers" / "flock_db.c"
ESP = ROOT / "esp32_companion" / "flock_companion" / "flock_companion.ino"

TRIPLE = re.compile(
    r"\{\s*0x([0-9a-fA-F]{2})\s*,\s*0x([0-9a-fA-F]{2})\s*,\s*0x([0-9a-fA-F]{2})\s*\}"
)


def extract(path: Path, symbol: str):
    """Pull the {0xaa,0xbb,0xcc} triples out of `symbol`'s initialiser."""
    src = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(
        re.escape(symbol) + r"\s*\[\s*\]\s*\[\s*3\s*\]\s*=\s*\{(.*?)\};", src, re.S
    )
    if not m:
        print(f"ERROR: could not find {symbol}[][3] in {path.relative_to(ROOT)}")
        print("       (the table was renamed or reshaped -- update this checker)")
        sys.exit(2)
    return [":".join(t).lower() for t in TRIPLE.findall(m.group(1))]


def compare(label, app_syms, esp_syms):
    a, e = extract(APP, app_syms), extract(ESP, esp_syms)
    if a == e:
        print(f"  OK   {label}: {len(a)} prefixes identical (order included)")
        return True

    print(f"  DRIFT {label}: {APP.name}={len(a)} vs {ESP.name}={len(e)}")
    only_app, only_esp = set(a) - set(e), set(e) - set(a)
    for p in sorted(only_app):
        print(f"        only in {APP.name}: {p}")
    for p in sorted(only_esp):
        print(f"        only in {ESP.name}: {p}")
    if not only_app and not only_esp:
        # Same members, different order. Harmless to matching, but the files are
        # meant to be diffable by eye, which is the whole hand-sync strategy.
        print("        same prefixes, DIFFERENT ORDER -- keep the rows aligned")
    return False


def main():
    print("OUI table parity: Flipper app vs ESP32 companion")
    ok = compare("Flock", "flock_ouis", "FLOCK_OUIS")
    ok &= compare("SoundThinking", "soundthinking_ouis", "SOUNDTHINKING_OUIS")
    if not ok:
        print("\nFAIL: the tables have drifted. Update BOTH files, keeping the")
        print("      row layout identical so they stay diffable by eye.")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
