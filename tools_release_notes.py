#!/usr/bin/env python3
"""Build a release body from docs/changelog.md for a given tag.

Releases were going out with an empty body (v2.9's was literally "-"), so the
one page most people land on said nothing about what changed. The changelog is
already written and already in the repo; this just lifts the matching section
and adds the two things a changelog entry does not cover: which .fap to
download, and how to install it.

    python3 tools_release_notes.py v3.0 > NOTES.md
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

PREAMBLE = """### Which file do I download?

Specter's API version is baked in at build time, and the Flipper's loader warns
when an app trails the firmware. One build cannot satisfy both firmware lines,
so there are two:

| Your firmware | Download |
|---|---|
| **Official** (flipperdevices) | **`specter.fap`** |
| **Unleashed / RogueMaster / Momentum** | **`specter-fw-dev.fap`** |

Copy it to `SD Card/apps/NFC/` (qFlipper, or drag it onto the SD card), then
find **Specter** under *Apps -> NFC*.

---

"""

FOOTER = """
---

Specter is **listen-only**: it reads the NFC chip's external-field detector and
never transmits. Use it only where you are authorised.
"""


def section(tag):
    version = tag.lstrip("v")
    text = open(os.path.join(HERE, "docs", "changelog.md")).read()
    # the entry runs from its own "## x.y" heading to the next one
    m = re.search(
        r"^##\s+" + re.escape(version) + r"\s*$(.*?)(?=^##\s|\Z)",
        text,
        re.M | re.S,
    )
    if not m:
        raise SystemExit(f"no changelog section for {version!r}")
    return m.group(1).strip()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: tools_release_notes.py <tag>")
    print(PREAMBLE + section(sys.argv[1]) + FOOTER)
