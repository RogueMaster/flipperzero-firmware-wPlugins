#!/usr/bin/env python3
# Full end-to-end IR transfer test: PORT_RX=receiver, PORT_TX=sender.
# Sends /ext/ir_share_tx.bin, observes both logs, then verifies the received
# file's MD5 on the receiver against the known source hash.
import sys, time, threading, os
sys.path.insert(0, '.')
from flip import Flip, PORT_RX, PORT_TX, log

SRC_MD5 = "d436d767fed83db89eacc70ed6cbd839"   # md5 of the 256-byte test file
OBS_SECS = int(sys.argv[1]) if len(sys.argv) > 1 else 45

def watchdog(sec):
    time.sleep(sec); log("WATCHDOG TIMEOUT"); os._exit(2)
threading.Thread(target=watchdog, args=(OBS_SECS + 90,), daemon=True).start()

log("open both")
rx = Flip(PORT_RX, "rx")
tx = Flip(PORT_TX, "tx")

log("launch both"); rx.launch(); tx.launch()

log("RX: menu Down -> OK (Receive)")
rx.key("down"); rx.key("ok"); time.sleep(1.5)   # receiver now listening

log("TX: menu OK (Send) -> browser")
tx.key("ok"); time.sleep(1.0)
log("TX: down x26 -> OK (select) -> OK (start)")
tx.keys("down", 26)
tx.key("ok"); time.sleep(0.9)
tx.key("ok"); time.sleep(1.0)

log(f"observe both logs ~{OBS_SECS}s")
rx.start_log(); tx.start_log()
time.sleep(OBS_SECS)
rx_log = rx.snap(); tx_log = tx.snap()
rx.stop_log(); tx.stop_log()

def show(tag, txt):
    log(f"===== {tag} =====")
    for line in txt.splitlines():
        L = line.strip()
        if any(k in L for k in ("ish_", "IrShare", "IrTransport", "MD5", "md5", "block",
                                 "ANNOUNCE", "REQUEST", "success", "finaliz", "Received")):
            log(L)
show("TX LOG", tx_log)
show("RX LOG", rx_log)

# Verify received file md5 on the receiver.
log("close apps");
try: rx.cmd("loader close", 1.0); tx.cmd("loader close", 1.0)
except Exception: pass
time.sleep(0.5)
res = rx.cmd("storage md5 /ext/inbox/ir_share_tx.bin", 2.0)
log("RX md5 result:", res.strip().replace("\n"," "))
got = None
for tok in res.split():
    if len(tok) == 32 and all(c in "0123456789abcdef" for c in tok):
        got = tok
log(f"SRC md5={SRC_MD5}")
log(f"GOT md5={got}")
log("VERDICT:", "PASS" if got == SRC_MD5 else "FAIL")
rx.close(); tx.close()
log("=== done ===")
