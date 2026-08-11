#!/usr/bin/env python3
"""
Stage 2 gate: the user's own meshlog.py must be happy with the emulator.

This is the real test of the fixture. meshlog.py is field code written against
actual nodes; if it fills its CSVs from the emulator, the protocol is right and
the Flipper's Logger — which reads the same events — has something faithful to
develop against.

Runs the emulator, points meshlog.py at it over TCP, waits for traffic, then
checks each CSV for plausible content rather than merely for existence.

    python tools/node-emulator/tests/gate_stage2.py [--seconds 45]
"""

from __future__ import annotations

import argparse
import csv
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(TOOLS))
EMULATOR = os.path.join(TOOLS, "meshcore_emulator.py")
MESHLOG = os.path.join(ROOT, "meshlog.py")

HOST = "127.0.0.1"
PORT = 5098

failures: list[str] = []
checks = 0


def check(condition: bool, description: str, detail: str = "") -> None:
    global checks
    checks += 1
    mark = "ok  " if condition else "FAIL"
    if not condition:
        failures.append(description)
    print(f"  {mark} {description}{(' -> ' + detail) if detail else ''}")


def read_csv(path: str) -> tuple[list[str], list[list[str]]]:
    if not os.path.exists(path):
        return [], []
    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.reader(handle))
    if not rows:
        return [], []
    return rows[0], rows[1:]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=45.0)
    args = parser.parse_args()

    print("stage 2 gate: meshlog.py against the emulator\n")

    if not os.path.exists(MESHLOG):
        print(f"  FAIL meshlog.py not found at {MESHLOG}")
        return 1

    outdir = tempfile.mkdtemp(prefix="meshlog-gate-")
    emulator = None
    logger = None

    try:
        emulator = subprocess.Popen(
            [sys.executable, EMULATOR, "--tcp", f"{HOST}:{PORT}", "--seed", "7"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        time.sleep(1.5)
        if emulator.poll() is not None:
            print("emulator exited early:\n" + (emulator.stdout.read() or ""))
            return 1

        # --ping exercises the send -> ACK round trip, which is what fills
        # ping.csv with an actual RTT.
        logger = subprocess.Popen(
            [
                sys.executable,
                MESHLOG,
                "--tcp",
                f"{HOST}:{PORT}",
                "--outdir",
                outdir,
                "--ping",
                "BASE",
                "--interval",
                "5",
                "--stats-interval",
                "10",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

        print(f"  .. collecting for {args.seconds:.0f}s into {outdir}")
        time.sleep(args.seconds)

        if logger.poll() is not None:
            print("meshlog.py exited early:\n" + (logger.stdout.read() or ""))
            return 1
    finally:
        for proc in (logger, emulator):
            if proc and proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()

    print()

    # ---- rx_log: the coverage map, and the Logger's primary source
    header, rows = read_csv(os.path.join(outdir, "rx_log.csv"))
    check(header == ["ts", "snr", "rssi", "lat", "lon", "acc", "raw"], "rx_log header", str(header))
    check(len(rows) >= 5, "rx_log collected packets", f"{len(rows)} rows")
    if rows:
        snrs = [float(r[1]) for r in rows if r[1] not in ("", None)]
        rssis = [float(r[2]) for r in rows if r[2] not in ("", None)]
        check(len(snrs) == len(rows), "every row carries an SNR")
        check(all(-25 <= v <= 20 for v in snrs), "SNR values are plausible dB", f"{min(snrs)}..{max(snrs)}")
        check(all(-140 <= v <= -30 for v in rssis), "RSSI values are plausible dBm", f"{min(rssis)}..{max(rssis)}")
        check(len(set(snrs)) > 1, "SNR varies rather than being a constant")
        check(all(r[6] for r in rows), "raw payload column is populated")

    # ---- telemetry
    header, rows = read_csv(os.path.join(outdir, "telemetry.csv"))
    check(
        header[:7] == ["ts", "batt_pct", "voltage", "noise_floor", "rx_total", "tx_total", "recv_errors"],
        "telemetry header",
        str(header[:7]),
    )
    check(len(rows) >= 2, "telemetry polled more than once", f"{len(rows)} rows")
    if rows:
        # Column is named batt_pct but the firmware sends millivolts; see the
        # emulator README. Checking for millivolts is checking reality.
        batt = [int(r[1]) for r in rows if r[1] not in ("", None)]
        check(bool(batt) and all(3000 <= v <= 4300 for v in batt), "battery reads as millivolts", str(batt[:3]))
        noise = [int(r[3]) for r in rows if r[3] not in ("", None)]
        check(bool(noise) and all(-140 <= v <= -60 for v in noise), "noise floor is plausible", str(noise[:3]))

    # ---- ping: the send -> ACK round trip
    header, rows = read_csv(os.path.join(outdir, "ping.csv"))
    check(header == ["ts", "target", "seq", "ok", "rtt_ms", "lat", "lon", "acc"], "ping header", str(header))
    check(len(rows) >= 2, "ping ran", f"{len(rows)} rows")
    ok_rows = [r for r in rows if r[3] == "1"]
    check(bool(ok_rows), "at least one ping was answered", f"{len(ok_rows)}/{len(rows)} ok")
    if ok_rows:
        rtts = [int(r[4]) for r in ok_rows if r[4]]
        check(bool(rtts) and all(0 < v < 3000 for v in rtts), "RTT is a sane millisecond figure", str(rtts[:4]))
        check(all(r[1] == "BASE" for r in rows), "ping target came from the contact list")

    # ---- events: adverts and incoming messages
    header, rows = read_csv(os.path.join(outdir, "events.csv"))
    check(header == ["ts", "type", "info", "lat", "lon", "acc", "raw"], "events header", str(header))
    kinds = {r[1] for r in rows}
    check(bool(rows), "events were recorded", f"{len(rows)} rows, kinds={sorted(kinds)}")

    print(f"\n{checks} checks, {len(failures)} failures")
    if failures:
        print("kept output for inspection: " + outdir)
        return 1

    shutil.rmtree(outdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
