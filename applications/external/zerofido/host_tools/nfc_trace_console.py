"""Tail ZeroFIDO NFC trace lines from the Flipper serial console.

The console reconnects automatically, optionally raises the firmware log level,
and filters output to the NFC trace tag by default so contactless APDU/ISO-DEP
debugging is easier to follow.
"""

from __future__ import annotations

import argparse
import glob
import sys
import time
from datetime import datetime
from pathlib import Path

import serial


TRACE_TAG = "ZeroFIDO:NFC"


def timestamp() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def resolve_port(port_arg: str) -> str | None:
    """Return an explicit port or choose the most likely Flipper CDC device."""
    if port_arg != "auto":
        return port_arg

    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        return None

    for port in ports:
        if "flip_" in port.lower():
            return port

    return ports[0]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Tail ZeroFIDO NFC trace lines from the Flipper USB CDC console."
    )
    parser.add_argument("-p", "--port", default="auto", help="CDC port path or 'auto'")
    parser.add_argument("-o", "--output", help="Optional file to append captured trace lines")
    parser.add_argument("--baudrate", type=int, default=230400, help="Serial baudrate")
    parser.add_argument(
        "--level",
        default="info",
        choices=["error", "warn", "info", "debug", "trace", "default"],
        help="Flipper CLI log level to request",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help=f"Print all CLI output instead of only lines containing {TRACE_TAG}",
    )
    parser.add_argument(
        "--no-log-command",
        action="store_true",
        help="Do not send the Flipper CLI 'log <level>' command after connecting",
    )
    return parser.parse_args()


def emit(line: str, output_file) -> None:
    """Write one line to stdout and the optional capture file."""
    sys.stdout.write(line)
    sys.stdout.flush()
    if output_file:
        output_file.write(line)
        output_file.flush()


def should_emit(line: str, include_all: bool) -> bool:
    """Filter serial output to ZeroFIDO NFC trace lines unless --all was set."""
    return include_all or TRACE_TAG in line


def tail_serial(args: argparse.Namespace, output_file) -> None:
    """Reconnect forever and stream decoded serial lines."""
    pending = ""

    while True:
        port = resolve_port(args.port)
        if not port:
            time.sleep(0.2)
            continue

        emit(f"\n[{timestamp()}] connected {port}\n", output_file)
        try:
            with serial.Serial(port, args.baudrate, timeout=0.25) as ser:
                if not args.no_log_command:
                    time.sleep(0.2)
                    ser.write(f"\r\nlog {args.level}\r\n".encode("utf-8"))
                    ser.flush()

                while True:
                    chunk = ser.read(4096)
                    if not chunk:
                        continue

                    pending += chunk.decode("utf-8", errors="replace")
                    while "\n" in pending:
                        line, pending = pending.split("\n", 1)
                        line = line + "\n"
                        if should_emit(line, args.all):
                            emit(line, output_file)
        except (OSError, serial.SerialException) as exc:
            emit(f"\n[{timestamp()}] disconnected {exc}\n", output_file)
            pending = ""
            time.sleep(0.2)


def main() -> int:
    args = parse_args()
    output_path = Path(args.output).expanduser() if args.output else None
    output_file = output_path.open("a", encoding="utf-8") if output_path else None

    try:
        tail_serial(args, output_file)
    except KeyboardInterrupt:
        return 130
    finally:
        if output_file:
            output_file.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
