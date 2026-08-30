#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt
"""
Fail if the OUI tables in the Flipper app and the ESP32 companion have drifted.

WHY THIS EXISTS. The same two tables are compiled into two different binaries
built by two different toolchains:

    helpers/flock_db.c                       flock_ouis[] and 7 more
    esp32_companion/.../flock_companion.ino  FLOCK_OUIS[] and 7 more

(Eight tables as of v0.77: Flock, SoundThinking, Axon, and the five
vendor-exclusive competitor tables -- Ubicquia, Motorola Solutions, Verkada,
Genetec, Avigilon. TABLES below is the single list; add a vendor there and every
check picks it up.)

There is no shared header -- the companion is an Arduino sketch that cannot
include the app's headers -- so both files carry a comment saying "keep these in
step by hand". Nothing enforced it. Editing one side alone silently desyncs
ESP-side scoring from the Flipper's, and the failure is invisible: detections
just quietly differ depending on which side saw the frame first.

A comment is not a guard. This is.

FOUR CHECKS, because parity alone proved insufficient:
  1. Content parity  -- the two tables hold the same prefixes in the same order.
  2. Declared count  -- each file's own "(31)" comment matches its array length.
  3. Retracted list  -- no upstream-retracted prefix has come back.
  4. Misattributed   -- no look-alike prefix belonging to a DIFFERENT company
                        (Motorola Mobility, GENETEC Corporation, Axon Networks)
                        has been added by someone grepping a vendor database.

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
    ("Axon", APP): None,
    ("Axon", ESP): re.compile(r"police equipment\s*\((\d+)\)"),
    # The five vendor-exclusive tables declare ONE combined count in each file,
    # checked by check_vendor_total() rather than per table -- five separate
    # count comments would be five more things to leave stale.
    ("Ubicquia", APP): None,
    ("Ubicquia", ESP): None,
    ("Motorola", APP): None,
    ("Motorola", ESP): None,
    ("Verkada", APP): None,
    ("Verkada", ESP): None,
    ("Genetec", APP): None,
    ("Genetec", ESP): None,
    ("Avigilon", APP): None,
    ("Avigilon", ESP): None,
}

# Prefixes a careless substring search WILL surface for a vendor we track, and
# that belong to a DIFFERENT company. None may ever enter a built-in table.
#
# The retracted-prefix check's sibling. It exists because the vendor-exclusive
# tables made this trap live rather than hypothetical: grep the IEEE registry for
# "motorola" and Motorola Mobility (consumer phones, a Lenovo company) comes back
# alongside Motorola Solutions; grep "genetec" and an unrelated Japanese company
# comes back alongside Genetec Inc. Adding one would repeat the 48:27:ea /
# a4:cf:12 failure -- a phone or chip vendor scored as surveillance hardware --
# on a far larger population than either of those.
MISATTRIBUTED = {
    "50:16:f4": "Motorola MOBILITY (Lenovo) -- consumer phones, not Motorola Solutions",
    "c4:a0:52": "Motorola MOBILITY (Lenovo) -- consumer phones, not Motorola Solutions",
    "c8:58:95": "Motorola MOBILITY (Lenovo) -- consumer phones, not Motorola Solutions",
    "00:0a:b1": "GENETEC Corporation (Japan) -- unrelated to Genetec Inc",
    "d8:c0:68": "Netgenetech Co. Ltd -- unrelated to Genetec Inc",
    "00:58:28": "Axon NETWORKS Inc -- unrelated to Axon Enterprise",
    "00:c0:d4": "Axon NETWORKS Inc -- unrelated to Axon Enterprise",
    "84:70:03": "Axon NETWORKS Inc -- unrelated to Axon Enterprise",
    "00:c0:c9": "ELSAG BAILEY PROCESS (industrial automation) -- not ALPR",
}

# Every table compared, in one place, so adding a fourth device class cannot land
# with only two of the three checks wired up.
TABLES = (
    ("Flock", "flock_ouis", "FLOCK_OUIS"),
    ("SoundThinking", "soundthinking_ouis", "SOUNDTHINKING_OUIS"),
    ("Axon", "axon_ouis", "AXON_OUIS"),
    ("Ubicquia", "ubicquia_ouis", "UBICQUIA_OUIS"),
    ("Motorola", "motorola_ouis", "MOTOROLA_OUIS"),
    ("Verkada", "verkada_ouis", "VERKADA_OUIS"),
    ("Genetec", "genetec_ouis", "GENETEC_OUIS"),
    ("Avigilon", "avigilon_ouis", "AVIGILON_OUIS"),
)

# The vendor-exclusive subset, which carries one shared declared count.
VENDOR_TABLES = TABLES[3:]

VENDOR_TOTAL_CLAIMS = {
    APP: re.compile(r"VENDOR-EXCLUSIVE OUIs\s*\((\d+)\s+across\s+(\d+)\s+vendors\)"),
    ESP: re.compile(r"Vendor-exclusive competitor OUIs\s*\((\d+)\s+across\s+(\d+)\s+vendors\)"),
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
        print(
            f"  OK   {label}: {path.name} comment says {declared}, array holds {actual}"
        )
        return True
    print(f"  STALE {label}: {path.name} comment says {declared}, array holds {actual}")
    print(
        "        Update the count comment, or the entry you added/removed is a mistake."
    )
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


def check_vendor_total(path):
    """Fail if a file's "(N across M vendors)" claim disagrees with its tables."""
    src = path.read_text(encoding="utf-8", errors="replace")
    m = VENDOR_TOTAL_CLAIMS[path].search(src)
    if not m:
        print(f"  MISS vendor total: no (N across M vendors) claim in {path.name}")
        print("        (the comment was reworded -- update VENDOR_TOTAL_CLAIMS here)")
        return False
    declared_n, declared_m = int(m.group(1)), int(m.group(2))
    sym_idx = 1 if path is APP else 2
    actual_n = sum(len(extract(path, t[sym_idx])) for t in VENDOR_TABLES)
    actual_m = len(VENDOR_TABLES)
    if (declared_n, declared_m) == (actual_n, actual_m):
        print(f"  OK   vendor total: {path.name} says {declared_n} across {declared_m}")
        return True
    print(
        f"  STALE vendor total: {path.name} says {declared_n} across {declared_m}, "
        f"tables hold {actual_n} across {actual_m}"
    )
    return False


def check_misattributed(label, path, symbol):
    """Fail if a known look-alike prefix from another company has been added."""
    found = [p for p in extract(path, symbol) if p in MISATTRIBUTED]
    if not found:
        return True
    for p in found:
        print(f"  MISATTRIBUTED {label}: {p} is in {path.name}")
        print(f"        {MISATTRIBUTED[p]}")
    return False


def main():
    print("OUI table parity: Flipper app vs ESP32 companion")
    ok = True
    for label, app_sym, esp_sym in TABLES:
        ok &= compare(label, app_sym, esp_sym)

    print("\nDeclared counts vs actual array length")
    for label, app_sym, esp_sym in TABLES:
        ok &= check_declared_count(label, APP, len(extract(APP, app_sym)))
        ok &= check_declared_count(label, ESP, len(extract(ESP, esp_sym)))

    print("\nVendor-exclusive declared totals")
    ok &= check_vendor_total(APP)
    ok &= check_vendor_total(ESP)

    print("\nRetracted prefixes (must be absent from both built-in tables)")
    retracted_ok = True
    for label, app_sym, esp_sym in TABLES:
        retracted_ok &= check_retracted(label, APP, app_sym)
        retracted_ok &= check_retracted(label, ESP, esp_sym)
    if retracted_ok:
        print(f"  OK   none of the {len(RETRACTED)} retracted prefixes are present")
    ok &= retracted_ok

    print("\nMisattributed look-alikes (wrong company, must never be added)")
    mis_ok = True
    for label, app_sym, esp_sym in TABLES:
        mis_ok &= check_misattributed(label, APP, app_sym)
        mis_ok &= check_misattributed(label, ESP, esp_sym)
    if mis_ok:
        print(f"  OK   none of the {len(MISATTRIBUTED)} look-alike prefixes are present")
    ok &= mis_ok

    if not ok:
        print("\nFAIL: the tables have drifted. Update BOTH files, keeping the")
        print("      row layout identical so they stay diffable by eye.")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
