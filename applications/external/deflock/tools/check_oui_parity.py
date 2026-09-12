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
    ("Utility", APP): None,
    ("Utility", ESP): None,
    ("DigitalAlly", APP): None,
    ("DigitalAlly", ESP): None,
    # Drones get their own count comment: they are outside the vendor-exclusive
    # subset above, so nothing else would ever check this table's size.
    ("Drone", APP): re.compile(r"Drone manufacturers\s*\((\d+)\)"),
    ("Drone", ESP): re.compile(r"Drone manufacturers\s*\((\d+)\)"),
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
    "fc:01:9e": "VIEVU -- real body cams, but Axon discontinued the line in 2018",
    "d4:2d:c5": "i-PRO Co Ltd -- body cams AND fixed cameras AND sensors; product not knowable from the OUI",
    "4c:48:da": "Beijing AUTELAN Technology (networking) -- not Autel Robotics",
    "00:1f:64": "Beijing Autelan Technology (networking) -- not Autel Robotics",
    "f8:40:68": "SZ DJI RONIN -- camera gimbals; DJI, but it does not fly",
    "20:1f:55": "DJI OSMO -- handheld cameras; DJI, but it does not fly",
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

# Prefixes the community lists carry that are TOO GENERIC to ever be evidence.
#
# Unlike RETRACTED (upstream tracked them, then withdrew them) these are still
# live in other projects' tables today -- zmattmanz/plume ships 54 across two
# tiers, simeononsecurity/flock-finder ships 31 -- so anyone widening our recall
# by importing from them WILL reintroduce these. Each was resolved against the
# IEEE MA-L registry on 2026-09-07; the organisation name is the whole argument.
#
# The cost is not theoretical. flock_db.c's own table comment records a Samsung
# T-Mobile hotspot being reported as a likely ALPR for doing nothing but scanning
# for networks -- a user had to report it. 48:27:ea is that same prefix family.
# And every Espressif entry would make FlipDeFlock detect its OWN companion
# board, which is an ESP32.
TOO_GENERIC = {
    "48:27:ea": "Samsung Electronics -- phones and hotspots; the exact FP class already field-reported",
    "a4:cf:12": "Espressif -- chip vendor, incl. our own companion board",
    "24:0a:c4": "Espressif -- chip vendor, incl. our own companion board",
    "dc:54:75": "Espressif -- chip vendor",
    "e0:e2:e6": "Espressif -- chip vendor",
    "68:b6:b3": "Espressif -- chip vendor",
    "a0:b7:65": "Espressif -- chip vendor",
    "a4:e5:7c": "Espressif -- chip vendor",
    "78:e3:6d": "Espressif -- chip vendor",
    "fc:f5:c4": "Espressif -- chip vendor",
    "b0:b2:1c": "Espressif -- chip vendor",
    "48:e7:29": "Espressif -- chip vendor",
    "c8:c9:a3": "Espressif -- chip vendor",
    "f0:9f:c2": "Ubiquiti -- consumer/prosumer access points",
    "8c:1f:64": "IEEE Registration Authority -- an MA-M/MA-S block shared by hundreds of companies; a 3-byte match here names nobody",
    "4c:6e:44": "IEEE Registration Authority -- shared block, same reason",
    "d8:a0:d8": "not registered in MA-L at all -- no organisation to attribute it to",
    "ec:5b:cd": "IEEE Registration Authority -- Autel Robotics holds only a /28 inside it",
    "e0:b6:f5": "IEEE Registration Authority -- Yuneec holds only a /28 inside it",
    "34:b5:f3": "IEEE Registration Authority -- Inspired Flight holds only a /28 inside it",
    "ac:86:d1": "IEEE Registration Authority -- Quantum Systems holds only a /28 inside it",
    "24:a1:0d": "IEEE Registration Authority -- Cyon Drones holds only a /28 inside it",
    "b4:4d:43": "IEEE Registration Authority -- UAV Navigation holds only a /28 inside it",
    "e8:b4:70": "IEEE Registration Authority -- Anduril holds only a /28 inside it",
    "18:d7:93": "IEEE Registration Authority -- shared with automotive diagnostics, not aircraft",
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
    ("Utility", "utility_ouis", "UTILITY_OUIS"),
    ("DigitalAlly", "digitalally_ouis", "DIGITALALLY_OUIS"),
    ("Drone", "drone_ouis", "DRONE_OUIS"),
)

# The vendor-exclusive COMPETITOR SURVEILLANCE subset, which carries one shared
# declared count ("N across M vendors").
#
# Drones are excluded from it and carry their own count instead. They are a
# different kind of thing -- an aircraft is not competitor ALPR kit -- and rolling
# them in would make that comment's number stop meaning what it says. Sliced by
# NAME rather than by index so appending a table cannot silently join this subset
# the way TABLES[3:] would have.
VENDOR_TABLES = tuple(
    t
    for t in TABLES
    if t[0]
    in (
        "Ubicquia",
        "Motorola",
        "Verkada",
        "Genetec",
        "Avigilon",
        "Utility",
        "DigitalAlly",
    )
)

VENDOR_TOTAL_CLAIMS = {
    APP: re.compile(r"VENDOR-EXCLUSIVE OUIs\s*\((\d+)\s+across\s+(\d+)\s+vendors\)"),
    ESP: re.compile(
        r"Vendor-exclusive competitor OUIs\s*\((\d+)\s+across\s+(\d+)\s+vendors\)"
    ),
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


def check_too_generic(label, path, symbol):
    """Fail if a shared chip-vendor / shared-block prefix has been added."""
    found = [p for p in extract(path, symbol) if p in TOO_GENERIC]
    if not found:
        return True
    for p in found:
        print(f"  TOO GENERIC {label}: {p} is in {path.name}")
        print(f"        {TOO_GENERIC[p]}")
    return False


def check_version_parity():
    """Fail if the companion's build version disagrees with FAP_VERSION.

    The two halves are built, flashed and tested as a PAIR. A companion left over
    from an older release is a leading cause of "it detects nothing", and until
    v0.88 the firmware carried no build identity at all -- the only label was
    whatever the .bin on the SD card had been named by hand, which cannot be
    checked after flashing and has been wrong.
    """
    fam = (ROOT / "application.fam").read_text(encoding="utf-8", errors="replace")
    m_app = re.search(r'FAP_VERSION\s*=\s*"([^"]+)"', fam)
    esp = ESP.read_text(encoding="utf-8", errors="replace")
    m_esp = re.search(r'#define\s+FLOCK_COMPANION_VERSION\s+"([^"]+)"', esp)
    if not m_app:
        print("  MISS version: no FAP_VERSION in application.fam")
        return False
    if not m_esp:
        print("  MISS version: no FLOCK_COMPANION_VERSION in flock_companion.ino")
        return False
    if m_app.group(1) != m_esp.group(1):
        print(
            f"  DRIFT version: app says {m_app.group(1)}, companion says {m_esp.group(1)}"
        )
        print("        They ship as a pair. Bump both in the same commit.")
        return False
    print(f"  OK   version: app and companion both {m_app.group(1)}")
    return True


def check_sv_wire_format():
    """The SV line's field count, on both sides of the wire.

    WHY THIS EXISTS. On 2026-09-10 the companion's SV printf was extended to
    carry fp2 and the printable IE signature, and the Flipper parser was
    extended to read them -- but the companion edit was silently lost before it
    reached disk. Both sides still COMPILED, both were flashed, and the only
    symptom was that the two new fields were quietly missing from every field
    report. Nothing caught it, because nothing compares a printf format against
    the parser that consumes it.

    A format string is not type-checked across a UART. This is the cheapest
    check that would have caught it.
    """
    print("")
    print("SV wire format: companion emitter vs Flipper parser")
    esp = ESP.read_text(encoding="utf-8", errors="ignore")
    app_parser = (ROOT / "helpers" / "esp_parser.c").read_text(
        encoding="utf-8", errors="ignore"
    )

    m = re.search(r'"(SV,%02x%02x%02x%02x%02x%02x[^"]*)', esp)
    if not m:
        print("  FAIL could not find the SV printf format in the companion")
        return False
    fmt = m.group(1)
    # The MAC is ONE field: the six %02x are concatenated with no commas between
    # them, so the field count is simply commas + 1.
    emitted = fmt.count(",") + 1
    # ANCHOR ON THE SV BRANCH. esp_parser.c declares a char* f[N] for every line
    # type it handles, and an unanchored search finds whichever appears first --
    # which is how the first version of this guard reported "3 fields on both
    # sides" and PASSED, while actually comparing the SV emitter against some
    # other line's parser. A guard that can pass by coincidence is not a guard.
    blk = re.search(r'"SV,", 3\) == 0\) \{(.*?)\n    \}', app_parser, re.S)
    if not blk:
        print("  FAIL could not find the SV branch in helpers/esp_parser.c")
        return False
    m2 = re.search(r"esp_split_fields\(line, f, (\d+)\)", blk.group(1))
    if not m2:
        print("  FAIL could not find the SV field split in helpers/esp_parser.c")
        return False
    parsed = int(m2.group(1))
    if emitted != parsed:
        print(
            "  FAIL SV: companion emits %d fields, parser splits %d" % (emitted, parsed)
        )
        print("       format: %s" % fmt)
        print("       Extend BOTH. The signature must stay LAST, because it is")
        print("       the only field allowed to contain commas.")
        return False
    print("  OK   SV: %d fields emitted and split on both sides" % emitted)
    return True


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
        print(
            f"  OK   none of the {len(MISATTRIBUTED)} look-alike prefixes are present"
        )
    ok &= mis_ok

    ok &= check_sv_wire_format()

    print()
    print("Shared chip-vendor / shared-block prefixes (never evidence on their own)")
    gen_ok = True
    for label, app_sym, esp_sym in TABLES:
        gen_ok &= check_too_generic(label, APP, app_sym)
        gen_ok &= check_too_generic(label, ESP, esp_sym)
    if gen_ok:
        print(f"  OK   none of the {len(TOO_GENERIC)} too-generic prefixes are present")
    ok &= gen_ok

    print()
    print("App / companion build version")
    ok &= check_version_parity()

    if not ok:
        print("\nFAIL: the tables have drifted. Update BOTH files, keeping the")
        print("      row layout identical so they stay diffable by eye.")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
