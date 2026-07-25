#!/usr/bin/env python3
"""Fail when Morse Flipper ELF allocations exceed hardware-tested limits."""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


ALLOC_SECTIONS = (".text", ".rodata", ".ARM.exidx", ".data", ".bss")
LIMITS = {
    "main": {"text": 78_000, "alloc": 85_500},
    "passive": {"text": 8_000, "alloc": 9_000},
    "passive_settings": {"text": 2_000, "alloc": 2_250},
    "tx_groups": {"text": 5_250, "alloc": 6_250},
    "settings": {"text": 12_000, "alloc": 13_500},
}


def section_sizes(path: Path) -> dict[str, int]:
    output = subprocess.run(
        ["size", "-A", str(path)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    sections: dict[str, int] = {}
    for line in output.splitlines():
        match = re.match(r"^(\.[A-Za-z0-9_.]+)\s+(\d+)\s+", line)
        if match:
            sections[match.group(1)] = int(match.group(2))
    if ".text" not in sections:
        raise ValueError(f"{path}: size output did not contain .text")
    return sections


def check(name: str, path: Path) -> bool:
    sections = section_sizes(path)
    text_size = sections[".text"]
    alloc_size = sum(sections.get(section, 0) for section in ALLOC_SECTIONS)
    limits = LIMITS[name]
    passed = text_size <= limits["text"] and alloc_size <= limits["alloc"]
    status = "PASS" if passed else "FAIL"
    print(
        f"{status} {name}: text={text_size}/{limits['text']} "
        f"alloc={alloc_size}/{limits['alloc']} bytes"
    )
    return passed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("/env/ufbt/build"),
        help="uFBT directory containing the debug ELFs",
    )
    args = parser.parse_args()
    paths = {
        "main": args.build_dir / "morse_flipper_d.elf",
        "passive": args.build_dir / "morse_flipper_passive_listening_d.elf",
        "passive_settings": args.build_dir / "morse_flipper_passive_settings_d.elf",
        "tx_groups": args.build_dir / "morse_flipper_tx_groups_d.elf",
        "settings": args.build_dir / "morse_flipper_settings_d.elf",
    }
    missing = [str(path) for path in paths.values() if not path.is_file()]
    if missing:
        parser.error("missing build output: " + ", ".join(missing))
    results = [check(name, path) for name, path in paths.items()]
    return 0 if all(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
