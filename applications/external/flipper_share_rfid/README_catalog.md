# Flipper Share RFID — direct file transfer between Flippers over the 125 kHz coil

> **⚠️ WARNING:** Flipper Share RFID is an **experimental-only** app, it is not recommended for regular use.
> Consider using other Flipper Share transports (NFC, Sub-GHz, IR) for everyday file transfer.

## Overview

**Flipper Share RFID** transfers a file from one Flipper Zero to another over the
built-in **125 kHz LF RFID coil** — no extra hardware, cables, phone, computer, internet
or radio needed. One Flipper drives the field and reads (receiver); the other
load-modulates the field like a tag (sender).

**⚠️ Hold the coils ~2 cm apart — do NOT press them together.** Pressed flat together
the coupling is too strong and nothing decodes; held steady at about 2 cm (a couple of
millimetres of spacer — a few stacked cards work) the transfer runs to completion.

Actual transfer speed is around **315 B/s** (32 KB in ~1:44). This is a slow, alignment-sensitive, one-way-on-the-air link — for real throughput use another Flipper Share transport.

Other Flipper Share transports (Sub-GHz, IR, NFC & more): [github.com/lomalkin/flipper-zero-apps](https://github.com/lomalkin/flipper-zero-apps)

Features:

- Works out of the box on any Flipper Zero — the RFID hardware is built in.
- Integrity check with an MD5 hash after reception; per-packet CRC16.
- Resumes automatically: separate and re-align the coils mid-transfer and the receiver's
  block bitmap picks up where it left off.
- One-way on the air (carousel): the sender broadcasts continuously, the receiver never
  transmits — so there is no pairing and no handshake.
- Torrent-like progress bar and ETA on the receiver; filename/size and ETA on the sender.
- No encryption (anyone with a reader in range could receive — don't send sensitive data).

Received files are saved to `/ext/inbox/`.

# Notes

See the full [README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_rfid/README.md) for the LF modem and protocol description.

Source code of the latest version is [here](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_rfid). Please feel free to open issues and PRs.

# Credits

Derived from Flipper Share. LF modem and transport built on the Flipper firmware
`furi_hal_rfid` raw reader/emulate API.
