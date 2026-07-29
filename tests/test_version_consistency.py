#!/usr/bin/env python3
"""Verify the promoted application metadata has one fixed version."""

from pathlib import Path
import re
import sys


EXPECTED_VERSION = "0.2.4"
ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"version consistency: {message}", file=sys.stderr)
    raise SystemExit(1)


def read_version() -> str:
    try:
        lines = [line.strip() for line in (ROOT / "VERSION").read_text().splitlines() if line.strip()]
    except OSError as error:
        fail(f"cannot read VERSION: {error}")
    if len(lines) != 1:
        fail(f"VERSION must contain exactly one non-empty line, found {len(lines)}")
    return lines[0]


def read_manifest_version() -> str:
    try:
        lines = (ROOT / "application.fam").read_text().splitlines()
    except OSError as error:
        fail(f"cannot read application.fam: {error}")
    active = [line.strip() for line in lines if line.strip().startswith("fap_version")]
    if len(active) != 1:
        fail(f"application.fam must contain exactly one active fap_version, found {len(active)}")
    match = re.fullmatch(r'fap_version\s*=\s*"([^"]+)"\s*,?', active[0])
    if match is None:
        fail(f"malformed fap_version assignment: {active[0]!r}")
    return match.group(1)


version = read_version()
manifest_version = read_manifest_version()
if version != EXPECTED_VERSION:
    fail(f"VERSION is {version!r}, expected {EXPECTED_VERSION!r}")
if manifest_version != EXPECTED_VERSION:
    fail(f"application.fam fap_version is {manifest_version!r}, expected {EXPECTED_VERSION!r}")
if version != manifest_version:
    fail("VERSION and application.fam do not match")

print(f"version consistency: {EXPECTED_VERSION}")
