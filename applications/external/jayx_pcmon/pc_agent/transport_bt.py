"""Bluetooth LE transport to Flipper Zero serial profile."""

from __future__ import annotations

import asyncio
import sys

# Flipper serial service UUIDs (from firmware serial_service_uuid.inc, LE reversed)
FLIPPER_SERIAL_SERVICE = "8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000"
FLIPPER_SERIAL_RX = "19ed82ae-ed21-4c9d-4145-228e62fe0000"  # PC writes here
FLIPPER_SERIAL_TX = "19ed82ae-ed21-4c9d-4145-228e61fe0000"
FLIPPER_SERIAL_FLOW = "19ed82ae-ed21-4c9d-4145-228e63fe0000"
FLIPPER_SERIAL_RPC = "19ed82ae-ed21-4c9d-4145-228e64fe0000"

NAME_HINTS = ("flipper", "jayx")


async def _find_device():
    from bleak import BleakScanner

    print("Scanning for Flipper BLE (8s)...")
    devices = await BleakScanner.discover(timeout=8.0, return_adv=True)

    for dev, adv in devices.values():
        uuids = [u.lower() for u in (adv.service_uuids or [])]
        if FLIPPER_SERIAL_SERVICE.lower() in uuids:
            print(f"Found by service: {dev.name or '?'} [{dev.address}]")
            return dev

    for dev, adv in devices.values():
        name = (dev.name or adv.local_name or "").lower()
        if any(h in name for h in NAME_HINTS):
            print(f"Found by name: {dev.name or adv.local_name} [{dev.address}]")
            return dev

    return None


def _find_char_uuid(client, *needles: str) -> str | None:
    needles_l = [n.lower() for n in needles]
    try:
        for svc in client.services:
            for char in svc.characteristics:
                cu = char.uuid.lower()
                for n in needles_l:
                    if cu == n or cu.endswith(n[-4:]):
                        return char.uuid
    except Exception:
        pass
    return None


async def _try_pair(client) -> None:
    """Bond/encrypt so AUTHEN serial characteristics can be written."""
    try:
        # Bleak >= 0.21 may accept protection_level on some backends
        try:
            await client.pair(protection_level=2)
            print("Paired (protection_level=2)")
            return
        except TypeError:
            pass
        await client.pair()
        print("Paired")
    except Exception as e:
        msg = str(e).lower()
        if any(
            s in msg
            for s in (
                "already",
                "exists",
                "bonded",
                "paired",
                "access denied",  # sometimes already bonded
            )
        ):
            print(f"Pair skipped ({e})")
        else:
            print(f"Pair warning: {e}")
            print("  If writes fail: Windows Bluetooth -> remove Flipper -> pair again")


async def _connect_client(device):
    from bleak import BleakClient

    # Prefer BLEDevice object (handles address type better than raw string)
    client = BleakClient(device, timeout=40.0)
    await client.connect()
    if not client.is_connected:
        raise RuntimeError("connect() returned but not connected")

    print(f"GATT connected to {getattr(device, 'address', device)}")
    await asyncio.sleep(0.3)
    await _try_pair(client)
    await asyncio.sleep(0.4)

    if not client.is_connected:
        raise RuntimeError("Disconnected during/after pair")

    return client


async def _write_packet(client, rx_uuid: str, packet: bytes) -> None:
    if not client.is_connected:
        raise RuntimeError("Not connected")
    # Prefer write-without-response; fall back to with-response
    try:
        await client.write_gatt_char(rx_uuid, packet, response=False)
    except Exception:
        await client.write_gatt_char(rx_uuid, packet, response=True)


async def _send_loop(packet_fn) -> None:
    while True:
        device = await _find_device()
        if device is None:
            print("No Flipper BLE device found. Retrying in 3s...")
            print("  On Flipper: JAYX -> Bluetooth -> OK (must stay on this screen)")
            await asyncio.sleep(3)
            continue

        client = None
        try:
            client = await _connect_client(device)

            rx_uuid = _find_char_uuid(client, FLIPPER_SERIAL_RX, "fe62") or FLIPPER_SERIAL_RX
            print(f"RX char: {rx_uuid}")

            # Optional: subscribe to flow-control so the stack keeps the link warm
            flow_uuid = _find_char_uuid(client, FLIPPER_SERIAL_FLOW, "fe63")
            if flow_uuid:
                try:

                    def _on_flow(_handle: int, data: bytearray) -> None:
                        pass

                    await client.start_notify(flow_uuid, _on_flow)
                    print("Flow-control notify on")
                except Exception as e:
                    print(f"Flow notify skipped: {e}")

            if not client.is_connected:
                raise RuntimeError("Lost link before first write")

            print("Link ready — streaming (live ~1s/tick; specs load in background)")
            ticks = 0
            while client.is_connected:
                try:
                    result = await asyncio.to_thread(packet_fn)
                except Exception as e:
                    print(f"Metrics error: {e}")
                    await asyncio.sleep(1)
                    continue

                if not client.is_connected:
                    print("Link dropped during metrics collect")
                    break

                packets = (
                    [bytes(result)]
                    if isinstance(result, (bytes, bytearray))
                    else list(result)
                )
                try:
                    for packet in packets:
                        await _write_packet(client, rx_uuid, packet)
                    ticks += 1
                    if ticks == 1 or ticks % 10 == 0:
                        print(f"OK — sent tick #{ticks} ({len(packets)} pkt)", flush=True)
                except Exception as e:
                    print(f"BLE write failed: {e}")
                    break

            print("Session ended, will rescan...")

        except Exception as e:
            print(f"BLE session error: {e}")
        finally:
            if client is not None:
                try:
                    if client.is_connected:
                        await client.disconnect()
                except Exception:
                    pass

        await asyncio.sleep(2)


def run_bt_loop(packet_fn) -> None:
    print("JAYX agent · Bluetooth mode")
    print("Checklist:")
    print("  1. Flipper: Bluetooth ON in Settings")
    print("  2. Flipper: open JAYX -> select Bluetooth -> OK (leave it open)")
    print("  3. Close Flipper mobile app")
    print("  4. Windows: Flipper paired under Bluetooth devices")
    if sys.platform == "win32":
        print("  5. If auth fails: remove Flipper in Windows Bluetooth, pair again, retry")
    try:
        asyncio.run(_send_loop(packet_fn))
    except KeyboardInterrupt:
        print("Stopped.")
