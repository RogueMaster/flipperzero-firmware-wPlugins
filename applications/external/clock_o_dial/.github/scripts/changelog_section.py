#!/usr/bin/env python3
"""
Print the changelog.md entry for one version, e.g. "1.1".

Entries follow the Apps Catalog format: a "1.1:" (or "v1.1:") header line,
followed by the entry text, up to the next version header.

Usage:
    python3 changelog_section.py <version> [changelog.md]

Exits non-zero if there's no non-empty entry for that version.
"""
import re
import sys

HEADER = re.compile(r"^v?(\d+\.\d+):\s*$")


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 1

    version = sys.argv[1]
    path = sys.argv[2] if len(sys.argv) == 3 else "changelog.md"
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()

    section = None
    for line in lines:
        m = HEADER.match(line)
        if m:
            if section is not None:
                break
            if m.group(1) == version:
                section = []
        elif section is not None:
            section.append(line)

    text = "\n".join(section or []).strip()
    if not text:
        print(f"No '{version}:' entry found in {path}", file=sys.stderr)
        return 1
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
