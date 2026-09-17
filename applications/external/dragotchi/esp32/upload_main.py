#!/usr/bin/env python3
"""Upload esp32/main.py to a running MicroPython board over its USB REPL.

Usage:  ./upload_main.py [REPL_PORT] [SRC]
Defaults: REPL_PORT=/dev/ttyACM1, SRC=the main.py next to this script.

The board must be running MicroPython (USB id 303a:4001), not in download
mode. Requires pyserial (ships with esptool's venv: pip install esptool).
"""
import os, sys, time, base64, serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM1"
srcfile = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "main.py")


def rd(s, t=0.4):
    time.sleep(t)
    return s.read(s.in_waiting or 1)


s = serial.Serial(port, 115200, timeout=1)
time.sleep(0.3)

# The running loop may be blocked in wlan.scan() for a few seconds; hammer
# Ctrl-C over ~6 s until we see a REPL prompt.
got_repl = False
for _ in range(12):
    s.write(b"\r\x03")
    if b">>>" in rd(s, 0.5) or b"KeyboardInterrupt" in rd(s, 0.1):
        got_repl = True
        break
if not got_repl:
    print("warning: never saw a REPL prompt; continuing anyway")

s.reset_input_buffer()
s.write(b"\r\x01")  # raw REPL
if b"raw REPL" not in rd(s, 0.6):
    print("warning: raw REPL banner not seen")

data = open(srcfile, "rb").read()
b64 = base64.b64encode(data).decode()
prog = (
    "import binascii\n"
    "f=open('main.py','wb')\n"
    "f.write(binascii.a2b_base64('%s'))\n"
    "f.close()\n"
    "print('WROTE', %d)\n" % (b64, len(data))
)
s.write(prog.encode())
s.write(b"\x04")  # execute
print(rd(s, 1.5).decode(errors="replace").strip())

s.write(b"\r\x02")  # normal REPL
rd(s, 0.3)
s.write(b"\r\x04")  # soft reset -> run the new main.py
time.sleep(0.5)
s.close()
print("uploaded %d bytes to %s and reset." % (len(data), port))
