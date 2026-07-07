#!/usr/bin/env python3
# Read-only live log capture from both Flippers while the user navigates by hand.
import sys, time, threading, os
sys.path.insert(0, '.')
from flip import Flip, PORT_RX, PORT_TX, log

DUR = int(sys.argv[1]) if len(sys.argv) > 1 else 150
KEYS = ("IrShare", "IrTransport", "ish_", "MD5", "md5", "block", "ANNOUNCE",
        "REQUEST", "DATA", "success", "finaliz", "Received", "Saved",
        "crash", "assert", "[E]", "furi_check", "Load", "elf", "trap")

def watchdog(sec):
    time.sleep(sec); log("WATCHDOG (capture end)"); os._exit(0)
threading.Thread(target=watchdog, args=(DUR + 30,), daemon=True).start()

log("opening ports...")
a = Flip(PORT_RX, "RX"); b = Flip(PORT_TX, "TX")
log("RX and TX ready; starting `log` on both")
a.start_log(); b.start_log()

state = {"RX": 0, "TX": 0}
def flush(dev, tag):
    s = dev.snap()
    nl = s.rfind("\n")
    if nl <= state[tag]:
        return
    chunk = s[state[tag]:nl]
    for line in chunk.splitlines():
        L = line.strip()
        if L and any(k in L for k in KEYS):
            log(f"[{tag}]", L)
    state[tag] = nl + 1

log(f"CAPTURING for {DUR}s — please navigate now.")
end = time.time() + DUR
while time.time() < end:
    time.sleep(0.6)
    flush(a, "RX"); flush(b, "TX")
a.close(); b.close()
log("=== capture done ===")
