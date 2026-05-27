#!/usr/bin/env python3
"""
Tail the Flipper serial CLI log so we can see what gap.c / the NUS profile
complain about when advertising starts.

Usage:
  python3 scripts/flipper-log.py                 # autodetect /dev/cu.usbmodemflip_*
  python3 scripts/flipper-log.py -p /dev/cu.xxx  # explicit port
  python3 scripts/flipper-log.py --debug         # enable "log debug" on the Flipper

Needs pyserial: pip3 install pyserial
"""

import argparse
import glob
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial not installed.  Run: pip3 install pyserial", file=sys.stderr)
    sys.exit(1)


def find_port() -> str | None:
    ports = sorted(glob.glob("/dev/cu.usbmodemflip_*"))
    return ports[0] if ports else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port")
    ap.add_argument("--debug", action="store_true",
                    help="send 'log debug' to the CLI before tailing")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        print("No /dev/cu.usbmodemflip_* port found. Is the Flipper plugged in?",
              file=sys.stderr)
        return 1
    print(f"Opening {port}…")

    with serial.Serial(port, baudrate=230400, timeout=1) as s:
        # Wake the CLI.
        s.write(b"\r\n")
        time.sleep(0.1)
        s.reset_input_buffer()

        # The Flipper CLI prints a banner + prompt ">: ".  Send the log
        # command at the default level (or debug if asked); then read
        # forever.
        cmd = b"log debug\r\n" if args.debug else b"log\r\n"
        s.write(cmd)
        print(f"Sent: {cmd.decode().strip()} — tailing log. Ctrl-C to quit.")
        print("-" * 72)

        try:
            while True:
                chunk = s.read(256)
                if chunk:
                    sys.stdout.write(chunk.decode(errors="replace"))
                    sys.stdout.flush()
        except KeyboardInterrupt:
            return 130


if __name__ == "__main__":
    sys.exit(main())
