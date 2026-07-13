#!/usr/bin/env python3
"""JAYX PC agent — stream live stats (+ specs sections) to Flipper."""

from __future__ import annotations

import argparse
import threading
import time

from metrics import collect
from protocol import pack_live, pack_specs_section
from specs import collect_spec_sections

SPECS_EVERY_S = 30.0
SPECS_SEND_EVERY_S = 8.0  # how often to push specs over the wire

_specs_lock = threading.Lock()
_specs_packets: list[bytes] = []
_specs_ready = False
_specs_thread_started = False
_last_specs_build = 0.0
_last_specs_send = 0.0


def _build_specs_packets(force: bool = False) -> list[bytes]:
    sections = collect_spec_sections(force=force)
    count = len(sections)
    out: list[bytes] = []
    for i, sec in enumerate(sections):
        out.append(
            pack_specs_section(
                section_index=i,
                section_count=count,
                title=sec["title"],
                lines=list(sec["lines"]),
            )
        )
    return out


def _specs_worker() -> None:
    """Heavy WMI work off the BLE/USB write path."""
    global _specs_packets, _specs_ready, _last_specs_build
    while True:
        try:
            print("Specs: collecting (WMI, may take a few seconds)...")
            t0 = time.monotonic()
            packets = _build_specs_packets(force=True)
            with _specs_lock:
                _specs_packets = packets
                _specs_ready = True
                _last_specs_build = time.monotonic()
            print(f"Specs: ready ({len(packets)} sections in {time.monotonic() - t0:.1f}s)")
        except Exception as e:
            print(f"Specs: error {e}")
        time.sleep(SPECS_EVERY_S)


def _ensure_specs_thread() -> None:
    global _specs_thread_started
    if _specs_thread_started:
        return
    _specs_thread_started = True
    t = threading.Thread(target=_specs_worker, name="jayx-specs", daemon=True)
    t.start()


def make_packets() -> list[bytes]:
    """
    Fast path: live metrics only (~1s from cpu_percent).
    Specs are built on a background thread and attached when ready.
    """
    _ensure_specs_thread()

    live = pack_live(**collect())
    packets: list[bytes] = [live]

    global _last_specs_send
    now = time.monotonic()
    with _specs_lock:
        if _specs_ready and _specs_packets and (now - _last_specs_send) >= SPECS_SEND_EVERY_S:
            packets.extend(_specs_packets)
            _last_specs_send = now

    return packets


def make_packet() -> bytes:
    return pack_live(**collect())


def main() -> int:
    parser = argparse.ArgumentParser(description="JAYX PC agent for Flipper Zero")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--usb", action="store_true", help="Stream over USB CDC serial")
    group.add_argument("--bt", action="store_true", help="Stream over Bluetooth LE")
    args = parser.parse_args()

    # Kick specs early so they may be ready by second tick
    _ensure_specs_thread()

    if args.usb:
        from transport_usb import run_usb_loop

        print("JAYX agent starting (USB)")
        run_usb_loop(make_packets)
    else:
        print("Bluetooth is still in development.")
        print("Check GitHub for updates.")
        print("Use USB for now:  python monitor.py --usb")
        return 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nBye")
        raise SystemExit(0)
