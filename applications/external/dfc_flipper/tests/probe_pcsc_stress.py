#!/usr/bin/env python3
"""Verify a large plain DESFire read through a PC/SC reader."""

import argparse
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tests.hardware.credential import load_read_fixtures  # noqa: E402
from tests.hardware.pcsc import run_plain_reads  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "credential",
        nargs="?",
        type=Path,
        default=ROOT / "tests/hardware/fixtures/ev3_plain_2k.dfc",
    )
    parser.add_argument("--reader", default="ACR1552")
    args = parser.parse_args()
    fixtures = load_read_fixtures(args.credential)
    result = run_plain_reads(args.reader, fixtures)
    print(f"ReadData OK: {result.bytes_read} bytes in {result.frames} frames")
    print(
        f"Readback SHA-256: {hashlib.sha256(b''.join(f.data for f in fixtures)).hexdigest()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
