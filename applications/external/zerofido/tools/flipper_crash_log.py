"""Continuously capture Flipper CDC console output for crash diagnostics."""

from __future__ import annotations

import argparse
import glob
import sys
import time
from datetime import datetime
from pathlib import Path

import serial


def timestamp() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def write_line(handle, line: str) -> None:
    """Write already-decoded console text to an open log handle."""
    handle.write(line)
    handle.flush()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Continuously reconnect to the Flipper CDC port and capture crash logs."
    )
    parser.add_argument("-p", "--port", default="auto", help="CDC port path or 'auto'")
    parser.add_argument(
        "-o",
        "--output",
        help="Optional log file. If omitted, logs are printed to stdout only.",
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=230400,
        help="Serial baudrate. Default: 230400",
    )
    return parser.parse_args()


def resolve_port(port_arg: str) -> str | None:
    """Return an explicit CDC port or choose the first likely Flipper port."""
    if port_arg != "auto":
        return port_arg

    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        return None

    for port in ports:
        if "flip_" in port.lower():
            return port

    return ports[0]


def main() -> int:
    """Reconnect forever until interrupted, mirroring console output to a file if requested."""
    args = parse_args()
    log_path = Path(args.output).expanduser() if args.output else None
    file_handle = log_path.open("a", encoding="utf-8") if log_path else None

    try:
        while True:
            port = resolve_port(args.port)
            if not port:
                time.sleep(0.2)
                continue

            banner = f"\n[{timestamp()}] Connected to {port}\n"
            sys.stdout.write(banner)
            sys.stdout.flush()
            if file_handle:
                write_line(file_handle, banner)

            try:
                with serial.Serial(port, args.baudrate, timeout=0.25) as ser:
                    while True:
                        chunk = ser.read(4096)
                        if not chunk:
                            continue

                        text = chunk.decode("utf-8", errors="replace")
                        sys.stdout.write(text)
                        sys.stdout.flush()
                        if file_handle:
                            write_line(file_handle, text)
            except (serial.SerialException, OSError) as exc:
                line = f"\n[{timestamp()}] Disconnected: {exc}\n"
                sys.stdout.write(line)
                sys.stdout.flush()
                if file_handle:
                    write_line(file_handle, line)
                time.sleep(0.2)
    finally:
        if file_handle:
            file_handle.close()


if __name__ == "__main__":
    raise SystemExit(main())
