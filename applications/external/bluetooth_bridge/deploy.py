#!/usr/bin/env python3
"""
Deploy BLE UART Bridge to Flipper Zero.

Three methods:
  usb      — via USB cable (ufbt launch)
  ble      — via BLE in-app self-update protocol (app must be running)
  rpc      — via BLE RPC file upload + app launch (app must NOT be running)

Usage:
  python3 deploy.py usb
  python3 deploy.py ble
  python3 deploy.py rpc
  python3 deploy.py auto   # try USB, then RPC, then BLE
"""

import asyncio
import os
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

FAP_PATH = Path(__file__).parent / "dist" / "bluetooth_bridge.fap"
FLIPPER_FAP_PATH = "/ext/apps/Bluetooth/bluetooth_bridge.fap"
REPO_DIR = Path(__file__).parent


def _generate_commit_hash_header():
    """Generate commit_hash.h with current git short hash."""
    try:
        h = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=REPO_DIR, text=True,
        ).strip()
    except Exception:
        h = "unknown"
    header = REPO_DIR / "commit_hash.h"
    header.write_text(f'#define COMMIT_HASH "{h}"\n')
    return h

# BLE constants
BLE_ADDRESS = "3E2917B6-34CA-6BF7-DE94-139B6824A6F0"
RX_CHAR = "19ed82ae-ed21-4c9d-4145-228e62fe0000"  # phone → Flipper (write)
TX_CHAR = "19ed82ae-ed21-4c9d-4145-228e61fe0000"  # Flipper → phone (indicate)
UPDATE_MAGIC = b"\x00FAP"


async def _find_flipper():
    """Find Flipper via BLE — tries address lookup, then full scan."""
    from bleak import BleakScanner
    dev = await BleakScanner.find_device_by_address(BLE_ADDRESS, timeout=10.0)
    if dev:
        return dev
    # Fallback: full scan, match by name
    devices = await BleakScanner.discover(timeout=10.0)
    for d in devices:
        if d.address == BLE_ADDRESS:
            return d
        if d.name and "Flipper" in d.name:
            return d
    return None


# ─── USB ────────────────────────────────────────────────────────────────────────

def deploy_usb():
    """Deploy via USB using ufbt launch."""
    print("=== USB deploy ===")
    if not FAP_PATH.exists():
        print(f"  Building first...")
        r = subprocess.run(["ufbt"], cwd=FAP_PATH.parent.parent, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"  Build failed:\n{r.stderr}")
            return False
        print(f"  Built OK")

    r = subprocess.run(
        ["ufbt", "launch"],
        cwd=FAP_PATH.parent.parent,
        capture_output=True, text=True, timeout=30,
    )
    print(r.stdout.strip())
    if r.returncode != 0:
        print(f"  Failed (exit {r.returncode}): {r.stderr.strip()}")
        return False
    print("  Deployed via USB!")
    return True


# ─── BLE in-app ─────────────────────────────────────────────────────────────────

async def deploy_ble():
    """Deploy via BLE in-app self-update protocol.

    The running app detects magic \\x00FAP + 4-byte LE size + 4-byte LE CRC32,
    then receives the raw .fap binary, writes it to flash, and relaunches.

    Uses write-without-response with 5ms pacing. Write-with-response is
    incompatible with the STM32WB BLE stack when indications are pending
    (causes disconnects due to ATT protocol collision).

    After full transfer, the app writes to SD card (~5s) then restarts.
    Disconnect 2+ seconds after transfer = success.
    """
    from bleak import BleakClient, BleakScanner

    print("=== BLE in-app deploy ===")
    fap_data = FAP_PATH.read_bytes()
    fap_size = len(fap_data)
    fap_crc = zlib.crc32(fap_data) & 0xFFFFFFFF
    print(f"  FAP: {fap_size} bytes, CRC32: {fap_crc:08x}")

    got_ok = False
    got_err = False

    def on_tx(_, data):
        nonlocal got_ok, got_err
        if b"OK" in data:
            got_ok = True
        if b"ER" in data:
            got_err = True

    disconnected_at = None
    send_done_at = None

    def on_disconnect(client):
        nonlocal disconnected_at
        disconnected_at = time.monotonic()

    dev = await _find_flipper()
    if not dev:
        print("  Flipper not found via BLE")
        return False

    print(f"  Connecting to {dev.name}...")
    async with BleakClient(dev, timeout=20.0, disconnected_callback=on_disconnect) as c:
        print(f"  Connected, MTU={c.mtu_size}")
        await c.start_notify(TX_CHAR, on_tx)

        # Drain — let pending TX indications clear
        print("  Draining 2s...")
        await asyncio.sleep(2)

        # Send header + data as write-without-response with 5ms pacing.
        # Avoids ATT indication/write-response collision on STM32WB BLE stack.
        header = UPDATE_MAGIC + struct.pack("<II", fap_size, fap_crc)
        payload = header + fap_data
        chunk_sz = min(c.mtu_size - 3, 400)
        sent = 0
        t0 = time.monotonic()

        while sent < len(payload):
            if disconnected_at:
                break
            end = min(sent + chunk_sz, len(payload))
            await c.write_gatt_char(RX_CHAR, payload[sent:end], response=False)
            sent = end
            await asyncio.sleep(0.005)

        elapsed = time.monotonic() - t0
        send_done_at = time.monotonic()
        print(f"  Sent {sent}/{len(payload)} in {elapsed:.1f}s")

        if sent < len(payload):
            print("  Transfer interrupted!")

        # Wait for OK/ER or timeout.  Disconnect early to avoid firmware
        # BLE-lifecycle crash that happens ~6s after connect.
        if sent >= len(payload) and not disconnected_at:
            print("  Waiting for OK (up to 4s)...")
            for _ in range(8):  # 4s max
                if got_ok or got_err or disconnected_at or not c.is_connected:
                    break
                await asyncio.sleep(0.5)
            # Disconnect explicitly before firmware crash window
            if c.is_connected:
                try:
                    await c.disconnect()
                except Exception:
                    pass

    if got_err:
        print("  FAILED: CRC32 mismatch — data corrupted in transit!")
        return False
    elif got_ok:
        print("  Update successful! (OK received)")
        return True
    elif disconnected_at and send_done_at and (disconnected_at - send_done_at) > 2.0:
        elapsed = disconnected_at - send_done_at
        print(f"  Update successful! (app restarted after {elapsed:.1f}s)")
        return True
    elif disconnected_at and sent >= len(payload):
        print("  Update likely successful (disconnect after full transfer)")
        return True
    else:
        print("  No response. App may not be running or data was corrupted.")
        return False


# ─── RPC BLE ────────────────────────────────────────────────────────────────────

async def deploy_rpc():
    """Deploy via Flipper system RPC over BLE.

    Works when app is NOT running. Uploads .fap to SD card and launches it.
    """
    from bleak import BleakClient, BleakScanner

    # Lazy import compiled protobuf
    rpc_dir = "/tmp/flipper-rpc"
    if rpc_dir not in sys.path:
        sys.path.insert(0, rpc_dir)
    import flipper_pb2

    print("=== RPC BLE deploy ===")
    fap_data = FAP_PATH.read_bytes()
    print(f"  FAP: {len(fap_data)} bytes -> {FLIPPER_FAP_PATH}")

    dev = await _find_flipper()
    if not dev:
        print("  Flipper not found via BLE")
        return False

    print(f"  Connecting to {dev.name}...")
    async with BleakClient(dev, timeout=20.0) as client:
        print(f"  Connected, MTU={client.mtu_size}")
        rpc = _FlipperRPC(client, flipper_pb2)
        await rpc.start()

        # RPC session opens automatically on BLE connect (firmware 1.4.3+)

        # Upload
        print("  Uploading...")
        ok = await rpc.storage_write(FLIPPER_FAP_PATH, fap_data)
        if not ok:
            print("  Upload failed!")
            return False
        print("  Upload OK!")

        # Launch
        print("  Launching app...")
        ok = await rpc.app_start(FLIPPER_FAP_PATH)
        if ok:
            print("  App launched!")
        else:
            print("  Launch failed (app may already be running)")

        await rpc.stop()
    return True


class _FlipperRPC:
    """Minimal Flipper RPC client over BLE serial."""

    WRITE_CHUNK = 512

    def __init__(self, client, pb2, debug=False):
        self.client = client
        self.pb2 = pb2
        self.debug = debug
        self.cmd_id = 0
        self.rx_buf = bytearray()
        self.raw_rx = bytearray()  # raw bytes before RPC session starts
        self.rpc_mode = False
        self.responses = asyncio.Queue()

    def _on_data(self, _, data: bytearray):
        if self.debug:
            print(f"\n    [RX {len(data)}b: {data[:40].hex()}]", end="", flush=True)
        if not self.rpc_mode:
            self.raw_rx.extend(data)
            return
        self.rx_buf.extend(data)
        self._try_parse()

    def _try_parse(self):
        while len(self.rx_buf) > 0:
            varint_val = 0
            shift = 0
            i = 0
            while i < len(self.rx_buf):
                byte = self.rx_buf[i]
                varint_val |= (byte & 0x7F) << shift
                shift += 7
                i += 1
                if not (byte & 0x80):
                    break
            else:
                return
            if len(self.rx_buf) < i + varint_val:
                return
            msg_data = bytes(self.rx_buf[i:i + varint_val])
            self.rx_buf = self.rx_buf[i + varint_val:]
            try:
                msg = self.pb2.Main()
                msg.ParseFromString(msg_data)
                self.responses.put_nowait(msg)
            except Exception as e:
                print(f"\n    [RPC parse error: {e}, len={len(msg_data)}, hex={msg_data[:20].hex()}]")
                # If parse fails, the varint might have been wrong.
                # Skip one byte and retry.
                continue

    async def start(self):
        await self.client.start_notify(TX_CHAR, self._on_data)
        self.rpc_mode = True

    async def stop(self):
        try:
            await self.client.stop_notify(TX_CHAR)
        except Exception:
            pass

    def _varint(self, v):
        buf = bytearray()
        while v > 0x7F:
            buf.append((v & 0x7F) | 0x80)
            v >>= 7
        buf.append(v & 0x7F)
        return bytes(buf)

    async def _send(self, msg):
        data = msg.SerializeToString()
        frame = self._varint(len(data)) + data
        chunk_sz = min(self.client.mtu_size - 3, 200)
        for i in range(0, len(frame), chunk_sz):
            await self.client.write_gatt_char(
                RX_CHAR, frame[i:i + chunk_sz], response=False)
            await asyncio.sleep(0.005)

    async def _recv(self, timeout=30.0):
        return await asyncio.wait_for(self.responses.get(), timeout=timeout)

    async def storage_write(self, path, data):
        total = len(data)
        sent = 0
        self.cmd_id += 1
        cmd_id = self.cmd_id
        while sent < total:
            end = min(sent + self.WRITE_CHUNK, total)
            is_last = (end >= total)
            msg = self.pb2.Main()
            msg.command_id = cmd_id
            msg.has_next = not is_last
            msg.storage_write_request.path = path
            msg.storage_write_request.file.data = data[sent:end]
            await self._send(msg)
            sent = end
            pct = sent * 100 // total
            print(f"\r    {sent}/{total} ({pct}%)", end="", flush=True)
            if not is_last:
                await asyncio.sleep(0.02)
        print()
        resp = await self._recv()
        return resp.command_status == 0

    async def app_start(self, name, args=""):
        self.cmd_id += 1
        msg = self.pb2.Main()
        msg.command_id = self.cmd_id
        msg.app_start_request.name = name
        if args:
            msg.app_start_request.args = args
        await self._send(msg)
        resp = await self._recv()
        return resp.command_status == 0


# ─── Auto ───────────────────────────────────────────────────────────────────────

async def deploy_auto():
    """Try USB first, then RPC, then BLE in-app."""
    print("=== Auto deploy ===\n")

    # 1. USB
    import glob
    if glob.glob("/dev/cu.usbmodem*"):
        print("USB device found, trying USB...")
        if deploy_usb():
            return True
        print()

    # 2. RPC (app not running)
    print("Trying RPC BLE (system-level upload)...")
    try:
        if await deploy_rpc():
            return True
    except Exception as e:
        print(f"  RPC failed: {e}")
    print()

    # 3. BLE in-app (app running)
    print("Trying BLE in-app self-update...")
    try:
        if await deploy_ble():
            return True
    except Exception as e:
        print(f"  BLE in-app failed: {e}")

    print("\nAll methods failed!")
    return False


# ─── Main ───────────────────────────────────────────────────────────────────────

def main():
    # Always regenerate commit_hash.h and rebuild
    h = _generate_commit_hash_header()
    print(f"  Commit: {h}")
    r = subprocess.run(["ufbt"], cwd=REPO_DIR, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  Build failed:\n{r.stderr}")
        sys.exit(1)

    method = sys.argv[1] if len(sys.argv) > 1 else "auto"

    if method == "usb":
        ok = deploy_usb()
    elif method == "ble":
        ok = asyncio.run(deploy_ble())
    elif method == "rpc":
        ok = asyncio.run(deploy_rpc())
    elif method == "auto":
        ok = asyncio.run(deploy_auto())
    else:
        print(f"Unknown method: {method}")
        print("Usage: deploy.py [usb|ble|rpc|auto]")
        sys.exit(1)

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
