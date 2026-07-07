#!/usr/bin/env python3
# Robust Flipper Zero CLI driver: a background reader thread continuously drains
# the serial port (so a left-over `log` stream can never wedge us), and commands
# just write + search the accumulated buffer.
import serial, threading, time, sys

# Device ports come from a local, gitignored config so no personal data is
# committed. Copy ports_local.example.py -> ports_local.py and set your ports
# (or export IR_RX_PORT / IR_TX_PORT).
try:
    from ports_local import PORT_RX, PORT_TX
except ImportError:
    import os
    PORT_RX = os.environ.get("IR_RX_PORT")
    PORT_TX = os.environ.get("IR_TX_PORT")

FAP = "/ext/apps/Infrared/flipper_share_ir.fap"

def log(*a):
    print(*a, flush=True)

class Flip:
    def __init__(self, port, name=""):
        if not port:
            raise SystemExit(
                "No Flipper port configured. Copy tools/ports_local.example.py to "
                "ports_local.py and set your ports (or export IR_RX_PORT / IR_TX_PORT).")
        self.name = name or port
        self.s = serial.Serial(port, timeout=0.1)
        self.buf = bytearray()
        self.lock = threading.Lock()
        self.alive = True
        self.t = threading.Thread(target=self._reader, daemon=True)
        self.t.start()
        time.sleep(0.2)
        self.s.write(b"\x03")   # Ctrl-C: leave any `log` streaming mode
        time.sleep(0.25)
        self.clear()
        self.s.write(b"\r\n")
        time.sleep(0.2)
        self.clear()

    def _reader(self):
        while self.alive:
            try:
                d = self.s.read(4096)
            except Exception:
                break
            if d:
                with self.lock:
                    self.buf += d

    def clear(self):
        with self.lock:
            self.buf.clear()

    def snap(self):
        with self.lock:
            return self.buf.decode(errors="replace")

    def send(self, line):
        self.s.write((line + "\r\n").encode())

    def cmd(self, line, wait=1.0):
        self.clear(); self.send(line); time.sleep(wait); return self.snap()

    def key(self, k, typ="short", settle=0.18):
        # Mimic a real button: Press -> Short/Long -> Release. Views that track
        # press/release state don't react to a lone Short event.
        self.send(f"input send {k} press"); time.sleep(0.04)
        self.send(f"input send {k} {typ}"); time.sleep(0.04)
        self.send(f"input send {k} release"); time.sleep(settle)

    def keys(self, k, n, settle=0.14):
        for _ in range(n):
            self.key(k, "short", settle)

    def launch(self):
        self.cmd("loader close", 1.2); time.sleep(0.3)
        self.cmd(f"loader open {FAP}", 1.5); time.sleep(1.2)

    def start_log(self):
        self.clear(); self.send("log"); time.sleep(0.3); self.clear()

    def collect(self, secs):
        time.sleep(secs); return self.snap()

    def stop_log(self):
        self.s.write(b"\x03"); time.sleep(0.2); self.clear()

    def close(self):
        self.alive = False
        time.sleep(0.15)
        try:
            self.s.write(b"\x03"); self.s.close()
        except Exception:
            pass
