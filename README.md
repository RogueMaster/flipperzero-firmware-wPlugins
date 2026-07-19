# Flipper Zero apps by lomalkin

Direct, hardware-free file transfer between two Flipper Zeros — send any file from one device to another with **no cables, phones, computers, internet or extra hardware**. Two flavors, same protocol, different transport.

---

## 📻 [Flipper Share](flipper_share) — over Sub-GHz

Transfers files using the internal CC1101 Sub-GHz transmitter. This is the relatively fast, general-purpose option.

- ~700 B/s (≈42 KB/min) — send an average `.fap` file or asset in under a minute
- Broadcast: multiple receivers can download at once

## 🔦 [Flipper Share IR](flipper_share_ir) — over Infrared

The same idea rewritten on top of a custom IR modem, using only the onboard IR LED and TSOP receiver.

- ~130 B/s (≈7.6 KB/min) — slower, half-duplex
- Symbol timings chosen so it won't trigger nearby IR-sensitive devices, like TVs, ACs, etc.
