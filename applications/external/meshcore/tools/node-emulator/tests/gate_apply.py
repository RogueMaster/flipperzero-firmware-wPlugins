#!/usr/bin/env python3
"""
Preset-apply gate: the exact command sequence the Flipper sends, over TCP.

The FAP's apply path is three commands and a read-back. The commands are built
by config/meshcore_apply.c, which the host tests already cover byte for byte —
what this adds is proof that a node *accepts* that sequence and that the
read-back reflects it. Together the two cover the whole path without a board.

Notably it exercises SET_PATH_HASH_MODE (61), which meshcore_c has no builder
for and which is easy to get wrong twice over: it is a separate command from
set-radio, and its second byte must be zero.

    python tools/node-emulator/tests/gate_apply.py
"""

from __future__ import annotations

import asyncio
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
EMULATOR = os.path.join(os.path.dirname(HERE), "meshcore_emulator.py")

HOST = "127.0.0.1"
PORT = 5097

# The built-in City/daily preset, in the units each layer wants.
PRESET_FREQ_MHZ = 868.731018
PRESET_BW_KHZ = 62.5
PRESET_SF = 7
PRESET_CR = 7
PRESET_PATH_HASH_BYTES = 2

CMD_SET_PATH_HASH_MODE = 61

failures: list[str] = []
checks = 0


def check(condition: bool, description: str, detail: str = "") -> None:
    global checks
    checks += 1
    mark = "ok  " if condition else "FAIL"
    if not condition:
        failures.append(description)
    print(f"  {mark} {description}{(' -> ' + detail) if detail else ''}")


async def run() -> None:
    from meshcore import EventType, MeshCore

    mc = await MeshCore.create_tcp(HOST, PORT)
    try:
        # Where the node starts, so "it changed" means something.
        before = (await mc.commands.send_device_query()).payload or {}
        check(
            before.get("path_hash_mode") == 0,
            "node starts with path hash mode 0",
            str(before.get("path_hash_mode")),
        )

        # --- step 1: radio
        result = await mc.commands.set_radio(PRESET_FREQ_MHZ, PRESET_BW_KHZ, PRESET_SF, PRESET_CR)
        check(result.type != EventType.ERROR, "SET_RADIO_PARAMS accepted", str(result.type))

        # --- step 2: path hash, its own command, mandatory zero second byte
        mode = PRESET_PATH_HASH_BYTES - 1
        result = await mc.commands.send(
            bytes([CMD_SET_PATH_HASH_MODE, 0, mode]), [EventType.OK, EventType.ERROR]
        )
        check(result.type == EventType.OK, "SET_PATH_HASH_MODE accepted", str(result.type))

        # --- step 3: name
        result = await mc.commands.set_name("ROVER-1")
        check(result.type == EventType.OK, "SET_ADVERT_NAME accepted", str(result.type))

        # --- verify: re-read and compare, which is what Apply's ticks mean
        info = (await mc.commands.send_appstart()).payload or {}
        check(
            abs(info.get("radio_freq", 0) - 868.731) < 0.0005,
            "frequency read back",
            str(info.get("radio_freq")),
        )
        check(abs(info.get("radio_bw", 0) - 62.5) < 0.0005, "bandwidth read back", str(info.get("radio_bw")))
        check(info.get("radio_sf") == PRESET_SF, "spreading factor read back", str(info.get("radio_sf")))
        check(info.get("radio_cr") == PRESET_CR, "coding rate read back", str(info.get("radio_cr")))
        check(info.get("name") == "ROVER-1", "name read back", repr(info.get("name")))

        device = (await mc.commands.send_device_query()).payload or {}
        check(
            device.get("path_hash_mode") == mode,
            "path hash read back from DEVICE_INFO",
            str(device.get("path_hash_mode")),
        )

        # --- the two ways to get command 61 wrong
        result = await mc.commands.send(
            bytes([CMD_SET_PATH_HASH_MODE, 0, 5]), [EventType.OK, EventType.ERROR]
        )
        check(result.type == EventType.ERROR, "a path hash mode of 5 is rejected", str(result.type))

        result = await mc.commands.send(
            bytes([CMD_SET_PATH_HASH_MODE, 1, 1]), [EventType.OK, EventType.ERROR]
        )
        check(
            result.type == EventType.ERROR,
            "a non-zero second byte is rejected",
            str(result.type),
        )

        # And the rejected attempts must not have changed anything.
        device = (await mc.commands.send_device_query()).payload or {}
        check(
            device.get("path_hash_mode") == mode,
            "a rejected command leaves the setting alone",
            str(device.get("path_hash_mode")),
        )
    finally:
        await mc.disconnect()


def main() -> int:
    print("preset apply gate: the Flipper's command sequence, over TCP\n")

    proc = subprocess.Popen(
        [sys.executable, EMULATOR, "--tcp", f"{HOST}:{PORT}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        time.sleep(1.5)
        if proc.poll() is not None:
            print("emulator exited early:\n" + (proc.stdout.read() if proc.stdout else ""))
            return 1
        asyncio.run(asyncio.wait_for(run(), timeout=30))
    except Exception as exc:  # noqa: BLE001 - the gate reports, it does not handle
        print(f"  FAIL client raised: {type(exc).__name__}: {exc}")
        failures.append(str(exc))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print(f"\n{checks} checks, {len(failures)} failures")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
