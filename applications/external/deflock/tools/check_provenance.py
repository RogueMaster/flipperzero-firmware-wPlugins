#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt
"""
Fail if any first-party file is missing its licence header or attribution.

WHY THIS EXISTS. This project has already been copied wholesale once: a v0.41
snapshot was re-uploaded under the same name with no attribution. No DMCA route
existed, and the reason was documentation, not law -- the MIT-era LICENSE read
"Copyright (c) 2026 FlipDeFlock contributors" with no personal name, and the copy
reproduced it verbatim, so the notice requirement was satisfied. Per-file SPDX
headers only arrived with the GPL relicense, AFTER that copy was taken.

So the headers are not decoration. They are the thing that makes the next
verbatim re-upload provably a stripped derivative rather than an honest fork, and
one untagged file is a hole in exactly that argument. Five had drifted in before
this check existed, including the shipped companion firmware source.

Checked, per first-party source file:
  1. an SPDX-License-Identifier line, matching the project licence
  2. a copyright line naming the maintainer

lib/ is excluded: it is vendored third-party code (jsmn, qrcodegen,
esp-serial-flasher) that carries its own upstream notices, and rewriting those
would be the very thing this file exists to object to.

Usage:  python tools/check_provenance.py     (exit 0 = clean, 1 = gaps)
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LICENCE = "GPL-3.0-or-later"
HOLDER = "ReconGrunt"
SUFFIXES = {".c", ".h", ".cpp", ".hpp", ".ino", ".py"}
# Vendored upstream code keeps its own notices -- see the module docstring.
EXCLUDE_PREFIXES = ("lib/",)
# How many lines from the top the header must appear within. Generous enough for
# a shebang plus a blank line, tight enough that a header buried mid-file (which
# a stripper would leave) does not count.
HEAD_LINES = 6


def tracked_sources():
    out = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.splitlines()
    for rel in out:
        if Path(rel).suffix not in SUFFIXES:
            continue
        if any(rel.startswith(p) for p in EXCLUDE_PREFIXES):
            continue
        yield rel


def main():
    print("Provenance: licence header + attribution on every first-party source")
    missing_spdx, missing_holder, wrong_licence = [], [], []

    files = sorted(tracked_sources())
    for rel in files:
        head = (ROOT / rel).read_text(encoding="utf-8", errors="replace").splitlines()[:HEAD_LINES]
        blob = "\n".join(head)
        if "SPDX-License-Identifier" not in blob:
            missing_spdx.append(rel)
        elif LICENCE not in blob:
            wrong_licence.append(rel)
        if HOLDER not in blob:
            missing_holder.append(rel)

    for label, items, hint in (
        ("no SPDX-License-Identifier", missing_spdx, f"add: SPDX-License-Identifier: {LICENCE}"),
        (f"SPDX is not {LICENCE}", wrong_licence, "the project is GPL-3.0-or-later"),
        (f"no '{HOLDER}' copyright line", missing_holder, f"add: Copyright (c) <year> {HOLDER}"),
    ):
        if items:
            print(f"\n  {len(items)} file(s) with {label}  --  {hint}")
            for rel in items:
                print(f"        {rel}")

    if missing_spdx or missing_holder or wrong_licence:
        print("\nFAIL: every first-party source must carry the licence and the")
        print("      attribution, in its first few lines. See the note at the top")
        print("      of this file for why that is load-bearing here.")
        return 1

    print(f"  OK   {len(files)} first-party sources, all tagged")
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
