# Flipper Share NFC — direct file transfer between Flippers over NFC

## Overview

**Flipper Share NFC** transfers any file directly from one Flipper Zero to another over the **NFC** channel (13.56 MHz, ISO14443-3A) — using only the onboard NFC antenna. No extra hardware, cables, phones, computers, internet or radio is needed.

Actual file transfer speed via **Flipper Share NFC** is around 7 KB/s — the fastest Flipper Share transport — but the two Flippers must be held together, back to back.

File size tested is 32 MB (~1 h 16 min), transfer time matches the ETA estimate. The protocol itself supports up to 4 GB (uint32_t file size). Maximum file size is limited by the Flipper Zero's free RAM on the receiver side (block map + progress state).

Other Flipper Share transports (Sub-GHz, IR & more): [github.com/lomalkin/flipper-zero-apps](https://github.com/lomalkin/flipper-zero-apps)

Features:

- Works out of the box on any Flipper Zero — the NFC hardware is built in.
- Integrity check with an MD5 hash after reception; per-packet CRC16 (on top of the ISO14443 CRC-A per frame).
- Automatic retransmission of lost/corrupted packets — the transfer continues "until success" (or until you restart it manually). Separating and re-touching the devices mid-transfer resumes where it left off.
- Torrent-like progress bar on the receiver; filename/size and ETA on the sender.
- The RF field is duty-cycled while idle and switched off after reception — cold antenna, negligible battery drain.

NFC is near-field: hold the two Flippers with their NFC antennas back to back aligned and touching for best results. The working gap is only a few centimeters. Received files are saved to **/ext/inbox/**.

# Notes

See the full [README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_nfc/README.md) for the NFC transport and protocol description.

Source code of the latest version is [here](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_nfc). Please feel free to open issues and PRs.

# Credits

Derived from Flipper Share. NFC transport built on the Flipper firmware NFC stack (ISO14443-3A poller/listener), all through the official external app API.
