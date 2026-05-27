#!/usr/bin/env python3
"""
Trigger macOS Bluetooth permission dialog for the Flipper bridge.

Run once so the OS grants BLE access to this process context.
The bridge daemon launched from the same terminal / app will inherit it.
"""

import asyncio
import sys


async def main():
    try:
        from bleak import BleakScanner
    except ImportError:
        print("bleak not installed — run:  pip install bleak  (or: pip install '.[bt]')")
        sys.exit(1)

    print("Requesting Bluetooth access from macOS…")
    print("(If a permission dialog appears, click Allow.)")

    try:
        # A short scan is enough to trigger the permission dialog.
        devices = await BleakScanner.discover(timeout=2.0)
    except Exception as e:
        print(f"Scan error: {e}")
        print("If you see a permission error, go to:")
        print("  System Settings → Privacy & Security → Bluetooth")
        print("  and allow access for this terminal / app.")
        sys.exit(1)

    flipper = [d for d in devices if d.name and d.name.startswith("Flipper")]
    if flipper:
        print(f"✓ Bluetooth access granted.  Found: {flipper[0].name}")
    else:
        print("✓ Bluetooth access granted.  (No Flipper nearby — that's fine.)")


if __name__ == "__main__":
    asyncio.run(main())
