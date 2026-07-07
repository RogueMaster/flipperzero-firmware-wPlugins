#!/usr/bin/env python3
import sys, time, threading, os
sys.path.insert(0, '.')
from flip import Flip, PORT_TX, log

def watchdog(sec):
    time.sleep(sec); log("WATCHDOG TIMEOUT"); os._exit(2)
threading.Thread(target=watchdog, args=(80,), daemon=True).start()

log("open TX"); f = Flip(PORT_TX, "tx")
log("info:", f.cmd("loader info", 1.0).strip().replace("\n", " ")[:120])
log("launch"); f.launch()
log("menu: OK (Send)"); f.key("ok"); time.sleep(1.0)
log("browser: down x26"); f.keys("down", 26)
log("select file: OK"); f.key("ok"); time.sleep(0.9)
log("show_file: OK (start send)"); f.key("ok"); time.sleep(1.2)
log("observe log ~7s")
f.start_log()
out = f.collect(7.0)
f.stop_log()
try: f.cmd("loader close", 1.0)
except Exception: pass
f.close()
log("=== relevant log lines ===")
for line in out.splitlines():
    L = line.strip()
    if any(k in L for k in ("ish_send", "IrShare", "IrTransport", "file selected", "md5", "SENDER", "fs_init", "ish_init")):
        log(L)
log("=== done ===")
