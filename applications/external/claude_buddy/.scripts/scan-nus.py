#!/usr/bin/env python3
"""
BLE scanner that looks for the Flipper's Nordic UART Service advertisement.

Prints everything a Claude Desktop-style picker would see:
  - Local name (primary adv OR scan response)
  - Service UUIDs advertised
  - Manufacturer data
  - RSSI
  - Whether this device would match the picker filter
    (name starts with "Claude" AND advertises NUS UUID)

Usage:
  python3 scripts/scan-nus.py          # scan for 15s, show matches
  python3 scripts/scan-nus.py --all    # print every advertiser seen
  python3 scripts/scan-nus.py -t 30    # scan for 30s
"""

import argparse
import asyncio
import sys
from bleak import BleakScanner

NUS_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"


def matches_claude_filter(name: str | None, service_uuids: list[str]) -> bool:
    has_name = bool(name) and name.lower().startswith("claude")
    has_nus = any(u.lower() == NUS_UUID for u in service_uuids)
    return has_name and has_nus


def fmt_device(device, adv) -> str:
    name = adv.local_name or device.name or "(no name)"
    uuids = adv.service_uuids or []
    mfg = adv.manufacturer_data or {}
    mfg_str = ", ".join(f"0x{k:04x}={v.hex()}" for k, v in mfg.items()) or "(none)"
    svc_data = adv.service_data or {}
    svc_data_str = (
        ", ".join(f"{k}={v.hex()}" for k, v in svc_data.items()) or "(none)"
    )
    tx_power = getattr(adv, "tx_power", None)
    platform_data = getattr(adv, "platform_data", None)

    claude_match = "✓ MATCH" if matches_claude_filter(name, uuids) else "  "
    has_nus = any(u.lower() == NUS_UUID for u in uuids)
    nus_mark = "[NUS]" if has_nus else "     "

    return (
        f"{claude_match} {nus_mark} {device.address}  rssi={adv.rssi:>4}  "
        f"tx_pw={tx_power}  name={name!r}\n"
        f"        uuids={uuids}\n"
        f"        mfg  ={mfg_str}\n"
        f"        svc  ={svc_data_str}\n"
        f"        platform_data={platform_data}"
    )


async def main(timeout: float, show_all: bool) -> int:
    seen: dict[str, tuple] = {}

    def cb(device, adv):
        key = device.address
        prev = seen.get(key)
        snapshot = (
            adv.local_name,
            tuple(adv.service_uuids or []),
            tuple(sorted(adv.manufacturer_data.items())) if adv.manufacturer_data else (),
            adv.rssi,
        )
        if prev == snapshot:
            return
        seen[key] = snapshot

        name = adv.local_name or device.name or ""
        uuids = adv.service_uuids or []
        flipper_hint = (
            name.lower().startswith("claude")
            or name.lower().startswith("flip")
            or "I13ch" in name
            or any(u.lower() == NUS_UUID for u in uuids)
            or 0x0483 in (adv.manufacturer_data or {})  # STMicro
        )
        if not show_all and not flipper_hint:
            return
        print(fmt_device(device, adv))
        print()

    print(f"Scanning for {timeout:.0f}s… (filter: name≈Claude* or NUS UUID)")
    print(f"NUS UUID: {NUS_UUID}")
    print("-" * 72)

    async with BleakScanner(detection_callback=cb) as scanner:
        await asyncio.sleep(timeout)

    matches = [
        (addr, snap) for addr, snap in seen.items()
        if matches_claude_filter(snap[0], list(snap[1]))
    ]
    print("-" * 72)
    print(f"Total advertisers seen: {len(seen)}")
    print(f"Would match Claude Desktop picker: {len(matches)}")
    for addr, snap in matches:
        print(f"  {addr}  name={snap[0]!r}")
    return 0 if matches else 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("-t", "--timeout", type=float, default=15.0)
    ap.add_argument("--all", action="store_true", help="print every advertiser")
    args = ap.parse_args()
    try:
        sys.exit(asyncio.run(main(args.timeout, args.all)))
    except KeyboardInterrupt:
        sys.exit(130)
