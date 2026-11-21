#!/usr/bin/env python3
"""Derive the Flipper Apps Catalog description from README.md.

The catalog accepts only a small subset of Markdown: no images, HTML, code
spans or deep headers. README.md may use those for GitHub inside blocks
fenced by these two comment lines:

    <!-- github-only:start -->
    <!-- github-only:end -->

This script drops those blocks and writes the rest to catalog/description.md,
which catalog/manifest.yml uses as the app description.

    python tools/catalog_description.py           write catalog/description.md
    python tools/catalog_description.py --check   fail if it is stale or invalid
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "README.md"
DESCRIPTION = ROOT / "catalog" / "description.md"

START = "<!-- github-only:start -->"
END = "<!-- github-only:end -->"

# What the catalog's Markdown filter rejects (tools/flipp_catalog/markdown_filter.py
# in flipperdevices/flipper-application-catalog).
FORBIDDEN = [
    (re.compile(r"`"), "code span or code block"),
    (re.compile(r"!\["), "image"),
    (re.compile(r"<"), "HTML or <autolink>"),
    (re.compile(r"&#?\w+;"), "HTML entity"),
    (re.compile(r"^#{3,}"), "header deeper than level 2"),
    (
        re.compile(r"^(={3,}|-{3,}|\*{3,}|_{3,})\s*$"),
        "horizontal line or underlined header",
    ),
    (re.compile(r"^>"), "block quote"),
    (re.compile(r"^( {4}|\t)"), "indented code block"),
    (re.compile(r"^\[[^\]]+\]:\s"), "reference-style link"),
]


def build(readme: str) -> str:
    kept: list[str] = []
    skipping = False
    for number, line in enumerate(readme.splitlines(), 1):
        marker = line.strip()
        if marker == START:
            if skipping:
                raise ValueError(f"README.md:{number}: nested {START}")
            skipping = True
        elif marker == END:
            if not skipping:
                raise ValueError(f"README.md:{number}: {END} without a start")
            skipping = False
        elif not skipping:
            # Removing a block leaves two blank lines; keep one
            if line.strip() or (kept and kept[-1].strip()):
                kept.append(line.rstrip())
    if skipping:
        raise ValueError(f"README.md: {START} is never closed")
    return "\n".join(kept).strip("\n") + "\n"


def problems(description: str) -> list[str]:
    found = []
    for number, line in enumerate(description.splitlines(), 1):
        for pattern, what in FORBIDDEN:
            if pattern.search(line):
                found.append(
                    f"line {number}: {what} is not allowed by the catalog: {line.strip()}"
                )
    return found


def main() -> int:
    check = sys.argv[1:] == ["--check"]
    if sys.argv[1:] and not check:
        print(__doc__)
        return 2

    try:
        description = build(README.read_text(encoding="utf-8"))
    except ValueError as error:
        print(error)
        return 1

    found = problems(description)
    if found:
        print(
            "The catalog description (README.md outside github-only blocks) is not valid:"
        )
        print("\n".join(found))
        return 1

    if check:
        current = (
            DESCRIPTION.read_text(encoding="utf-8") if DESCRIPTION.exists() else ""
        )
        if current != description:
            print(
                "catalog/description.md is out of date. Run: python tools/catalog_description.py"
            )
            return 1
        print("catalog/description.md matches README.md")
        return 0

    DESCRIPTION.write_text(description, encoding="utf-8", newline="\n")
    print(f"Wrote {DESCRIPTION.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
