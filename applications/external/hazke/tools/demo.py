"""Play a scripted demo on a Flipper over its USB CLI, for capturing clips.

Usage: python3 demo.py <port> <script>

Script lines (blank lines and # comments are skipped):
  tap KEY [N]        press and release KEY, N times (default 1)
  hold KEY SECONDS   hold KEY down
  wait SECONDS       do nothing
KEY is one of up, down, left, right, ok, back. Needs pyserial.

Before every line the driver checks that Hazke is still the running app
and stops if it isn't, so a script that goes wrong never sends buttons to
the Flipper's own menus.
"""

import sys
import time

import serial

GAP = 0.12  # seconds between taps


class Cli:
    def __init__(self, port):
        self.s = serial.Serial(port, 230400, timeout=0.05)
        self.s.write(b"\x03\r")
        time.sleep(0.8)
        self.s.read(65536)

    def hazke_running(self):
        self.s.read(65536)
        self.s.write(b"loader info\r")
        out, end = b"", time.time() + 0.6
        while time.time() < end and b"running" not in out:
            out += self.s.read(65536)
        return b'"Hazke" is running' in out

    def send(self, key, kind):
        self.s.write(f"input send {key} {kind}\r".encode())
        time.sleep(0.03)
        self.s.read(65536)


def run(port, path):
    cli = Cli(port)
    for n, raw in enumerate(open(path), 1):
        line = raw.split("#", 1)[0].split()
        if not line:
            continue
        cmd, args = line[0], line[1:]
        if cmd != "wait" and not cli.hazke_running():
            sys.exit(f"{path}:{n}: Hazke is not running, stopping")
        if cmd == "tap":
            for _ in range(int(args[1]) if len(args) > 1 else 1):
                cli.send(args[0], "press")
                time.sleep(0.05)
                cli.send(args[0], "release")
                time.sleep(GAP)
        elif cmd == "hold":
            cli.send(args[0], "press")
            time.sleep(float(args[1]))
            cli.send(args[0], "release")
            time.sleep(GAP)
        elif cmd == "wait":
            time.sleep(float(args[0]))
        else:
            sys.exit(f"{path}:{n}: unknown command {cmd!r}")
        print(f"{n}: {' '.join(line)}", flush=True)


if __name__ == "__main__":
    run(sys.argv[1], sys.argv[2])
