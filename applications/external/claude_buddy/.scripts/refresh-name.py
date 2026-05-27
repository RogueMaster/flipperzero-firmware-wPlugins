#!/usr/bin/env python3
"""
Connect to the Flipper and read the GATT Device Name characteristic
(0x2A00), which forces macOS/CoreBluetooth to refresh its cached
CBPeripheral.name.

Usage:
  python3 scripts/refresh-name.py
"""

import asyncio
import sys

from bleak import BleakClient, BleakScanner

NUS_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
DEVICE_NAME_CHAR = "00002a00-0000-1000-8000-00805f9b34fb"


async def main() -> int:
    print("Scanning 8s for a NUS peripheral…")
    target = None
    async with BleakScanner() as scanner:
        await asyncio.sleep(8)
        for d, adv in scanner.discovered_devices_and_advertisement_data.values():
            uuids = [u.lower() for u in (adv.service_uuids or [])]
            name = adv.local_name or d.name or ""
            if NUS_UUID in uuids or name.lower().startswith("claude"):
                target = d
                print(f"  candidate: {d.address}  adv_name={adv.local_name!r}  "
                      f"cached_name={d.name!r}  uuids={uuids}")
                break

    if not target:
        print("No NUS/Claude device found.", file=sys.stderr)
        return 1

    print(f"Connecting to {target.address}…")
    async with BleakClient(target) as client:
        print(f"  connected: {client.is_connected}")
        try:
            data = await client.read_gatt_char(DEVICE_NAME_CHAR)
            print(f"  GATT Device Name (0x2A00) = {data.decode(errors='replace')!r}")
        except Exception as e:
            print(f"  could not read 0x2A00: {e}")
        # Also list services so macOS caches them.
        for s in client.services:
            print(f"  svc {s.uuid}")
    print("Disconnected. Re-run scan-nus.py now to see if cached name updated.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(asyncio.run(main()))
    except KeyboardInterrupt:
        sys.exit(130)
