#!/usr/bin/env python3
"""
Stage 1 gate: the reference client must accept the emulator as a real node.

Starts the emulator on a loopback TCP port, connects with the `meshcore`
library that meshlog.py itself uses, and checks that the replies decode into
sane values. If this passes, the framing and the reply layouts are right —
which is the whole point of the fixture.

    python tools/node-emulator/tests/gate_stage1.py
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
PORT = 5099

failures: list[str] = []
checks = 0


def check(condition: bool, description: str, detail: str = "") -> None:
    global checks
    checks += 1
    if condition:
        print(f"  ok   {description}{(' -> ' + detail) if detail else ''}")
    else:
        failures.append(description)
        print(f"  FAIL {description}{(' -> ' + detail) if detail else ''}")


async def run_client() -> None:
    from meshcore import MeshCore

    mc = await MeshCore.create_tcp(HOST, PORT)
    try:
        info = (await mc.commands.send_appstart()).payload or {}
        check(bool(info), "APP_START returns self-info", str(sorted(info)[:4]))
        check(info.get("name") == "EMU-1", "node name round-trips", repr(info.get("name")))
        # The two radio fields use different scales on the wire; the client
        # divides both by 1000, so these are MHz and kHz respectively.
        check(
            abs(info.get("radio_freq", 0) - 869.525) < 0.001,
            "frequency decodes as MHz",
            str(info.get("radio_freq")),
        )
        check(
            abs(info.get("radio_bw", 0) - 250.0) < 0.001,
            "bandwidth decodes as kHz",
            str(info.get("radio_bw")),
        )
        check(info.get("radio_sf") == 10, "spreading factor", str(info.get("radio_sf")))
        check(len(info.get("public_key", "")) == 64, "public key is 32 bytes")

        dev = (await mc.commands.send_device_query()).payload or {}
        check("T114" in str(dev.get("model", "")), "model identifies the board", str(dev.get("model")))
        check(dev.get("ver") == "v1.12.0", "firmware version string", str(dev.get("ver")))
        check(dev.get("max_contacts") == 350, "max_contacts is doubled on the wire")

        bat = (await mc.commands.get_bat()).payload or {}
        level = bat.get("level")
        check(level is not None, "battery replies")
        check(
            isinstance(level, int) and 3000 <= level <= 4300,
            "battery is a plausible millivolt reading",
            str(level),
        )

        radio = (await mc.commands.get_stats_radio()).payload or {}
        check(
            isinstance(radio.get("noise_floor"), int) and -130 < radio["noise_floor"] < -60,
            "radio stats give a plausible noise floor",
            str(radio.get("noise_floor")),
        )

        pkts = (await mc.commands.get_stats_packets()).payload or {}
        check("recv_errors" in pkts, "packet stats include recv_errors", str(sorted(pkts)[:3]))

        contacts = (await mc.commands.get_contacts()).payload or {}
        check(len(contacts) == 4, "contact list arrives complete", f"{len(contacts)} contacts")
        names = {c.get("adv_name") for c in contacts.values()} if isinstance(contacts, dict) else set()
        check("BASE" in names, "contacts carry their names", str(sorted(names)))
    finally:
        await mc.disconnect()


def main() -> int:
    print("stage 1 gate: reference meshcore client against the emulator\n")

    proc = subprocess.Popen(
        [sys.executable, EMULATOR, "--tcp", f"{HOST}:{PORT}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        # Give the listener a moment; the client's own retry is unforgiving.
        time.sleep(1.5)
        if proc.poll() is not None:
            print("emulator exited early:\n" + (proc.stdout.read() if proc.stdout else ""))
            return 1

        asyncio.run(asyncio.wait_for(run_client(), timeout=30))
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
