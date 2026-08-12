#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt
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

THREE CHECKS, because parity alone proved insufficient:
  1. Content parity  -- the two tables hold the same prefixes in the same order.
  2. Declared count  -- each file's own "(31)" comment matches its array length.
  3. Retracted list  -- no upstream-retracted prefix has come back.

(2) and (3) were added after f8:a2:d6 shipped in v0.67-v0.71: check (1) passed
throughout, because the commit that re-added it changed BOTH files identically
while both count comments still said 31. A check that only compares the two
copies to each other cannot see a mistake made in both.

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


# ---------------------------------------------------------------------------
# Prefixes upstream tracked and then RETRACTED. Re-adding one is always a bug,
# never a rediscovery -- the flat colonelpanichacks/flock-you list still carries
# some of them and has no status column to record the doubt, so anyone importing
# from it reintroduces exactly these.
#
# THIS IS THE LOAD-BEARING CHECK. f8:a2:d6 was dropped for v0.44 and then
# silently re-added by 93beede (2026-08-05) while the tables were reflowed. It
# shipped in v0.67-v0.71 scoring "Likely" on any wildcard probe -- and the parity
# check below stayed green the entire time, because that commit drifted BOTH
# sides identically. Content parity alone cannot see a shared mistake.
RETRACTED = {
    "f8:a2:d6": 'Removed upstream: "low confidence; hit on a Sony Media Player"',
    "6c:cd:d6": 'Removed upstream: "Nope - Netgear" (misattributed)',
    "94:2a:6f": 'Removed upstream: "Nope - Ubiquiti" (misattributed)',
    "f4:e2:c6": 'Removed upstream: "Nope - Ubiquiti" (misattributed)',
    "cc:cc:cc": 'Removed upstream: "No clue; no hits"',
    "00:0c:e7": 'Removed upstream: MediaTek, "possible false positive"',
}

# Where each file states how many prefixes its table holds. A count comment that
# disagrees with the array is the tell that something was added or removed
# without the author noticing -- both files said 31 for five releases while the
# arrays held 32, and nothing compared the two numbers. None = no declared count.
COUNT_CLAIMS = {
    ("Flock", APP): re.compile(r"^\s*\*\s*(\d+)\s+OUI prefixes\b", re.M),
    ("Flock", ESP): re.compile(r"Flock-associated OUI prefixes\s*\((\d+)\)"),
    ("SoundThinking", APP): None,
    ("SoundThinking", ESP): re.compile(r"acoustic sensors\s*\((\d+)\)"),
}


def check_declared_count(label, path, actual):
    """Fail if the file's own count comment disagrees with the array length."""
    pattern = COUNT_CLAIMS.get((label, path))
    if pattern is None:
        return True
    src = path.read_text(encoding="utf-8", errors="replace")
    m = pattern.search(src)
    if not m:
        print(f"  MISS {label}: no count comment found in {path.name}")
        print("        (the comment was reworded -- update COUNT_CLAIMS here)")
        return False
    declared = int(m.group(1))
    if declared == actual:
        print(f"  OK   {label}: {path.name} comment says {declared}, array holds {actual}")
        return True
    print(f"  STALE {label}: {path.name} comment says {declared}, array holds {actual}")
    print("        Update the count comment, or the entry you added/removed is a mistake.")
    return False


def check_retracted(label, path, symbol):
    """Fail if any upstream-retracted prefix is present in a built-in table."""
    found = [p for p in extract(path, symbol) if p in RETRACTED]
    if not found:
        return True
    for p in found:
        print(f"  RETRACTED {label}: {p} is back in {path.name}")
        print(f"        {RETRACTED[p]}")
    return False


def main():
    print("OUI table parity: Flipper app vs ESP32 companion")
    ok = compare("Flock", "flock_ouis", "FLOCK_OUIS")
    ok &= compare("SoundThinking", "soundthinking_ouis", "SOUNDTHINKING_OUIS")

    print("\nDeclared counts vs actual array length")
    for label, app_sym, esp_sym in (
        ("Flock", "flock_ouis", "FLOCK_OUIS"),
        ("SoundThinking", "soundthinking_ouis", "SOUNDTHINKING_OUIS"),
    ):
        ok &= check_declared_count(label, APP, len(extract(APP, app_sym)))
        ok &= check_declared_count(label, ESP, len(extract(ESP, esp_sym)))

    print("\nRetracted prefixes (must be absent from both built-in tables)")
    retracted_ok = True
    for label, app_sym, esp_sym in (
        ("Flock", "flock_ouis", "FLOCK_OUIS"),
        ("SoundThinking", "soundthinking_ouis", "SOUNDTHINKING_OUIS"),
    ):
        retracted_ok &= check_retracted(label, APP, app_sym)
        retracted_ok &= check_retracted(label, ESP, esp_sym)
    if retracted_ok:
        print(f"  OK   none of the {len(RETRACTED)} retracted prefixes are present")
    ok &= retracted_ok

    if not ok:
        print("\nFAIL: the tables have drifted. Update BOTH files, keeping the")
        print("      row layout identical so they stay diffable by eye.")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
