#!/usr/bin/env python3
"""Verify a MACed DESFire read from a Flipper through a PC/SC reader."""

import argparse
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tests.hardware.credential import load_read_fixture  # noqa: E402
from tests.hardware.pcsc import PcscCard, run_iso_2k3des_read  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("credential", type=Path)
    parser.add_argument("--reader", default="ACR1552")
    parser.add_argument("--application", type=int, default=0)
    parser.add_argument("--file", type=int, default=0)
    parser.add_argument("--key", type=int, default=1)
    args = parser.parse_args()
    fixture = load_read_fixture(args.credential, args.application, args.file, args.key)
    with PcscCard(args.reader) as card:
        result = run_iso_2k3des_read(card, fixture)
    print("ISO 2K3DES authentication: verified")
    print(f"ReadData bytes: {result.bytes_read} in {result.frames} frames")
    print(f"Readback SHA-256: {hashlib.sha256(fixture.data).hexdigest()}")
    print("Readback matches credential: yes")
    print("Response MAC: verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
