# Flipper Share

Direct, hardware-free file transfer between two Flipper Zeros — send any file from one device to another with **no cables, phones, computers, internet or extra hardware**. One protocol (resumable, integrity-checked), different transports.

News & updates: Telegram [@flipper_share](https://t.me/flipper_share)

---

## 📻 [Flipper Share](flipper_share) — over Sub-GHz

Transfers files using the internal CC1101 Sub-GHz transmitter. The general-purpose option.

- ~700 B/s — send an average `.fap` file or asset in under a minute
- Works over distance; broadcast: multiple receivers can download at once
- Install: [lab.flipper.net/apps/flipper_share](https://lab.flipper.net/apps/flipper_share)

## 🔦 [Flipper Share IR](flipper_share_ir) — over Infrared

The same idea rewritten on top of a custom IR modem, using only the onboard IR LED and TSOP receiver.

- ~130 B/s — slower, half-duplex
- Works up to ~30 m line-of-sight without ambient IR interference — that is even farther than Sub-GHz with the stock antennas
- Symbol timings chosen so it won't trigger nearby IR-sensitive devices, like TVs, ACs, etc.
- Install: [lab.flipper.net/apps/flipper_share_ir](https://lab.flipper.net/apps/flipper_share_ir)

## 💳 [Flipper Share NFC](flipper_share_nfc) — over NFC

The fastest flavor: an ISO14443-3A link between two Flippers held antenna to antenna.

- ~7 KB/s — the fastest Flipper Share transport
- Works in contact (antennas together); an interrupted transfer resumes automatically when the devices touch again
- Install: [lab.flipper.net/apps/flipper_share_nfc](https://lab.flipper.net/apps/flipper_share_nfc)
