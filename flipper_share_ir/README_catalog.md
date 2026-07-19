# Flipper Share IR — direct file transfer between Flippers over IR

## Overview

**Flipper Share IR** transfers any file directly from one Flipper Zero to another over the **Infrared** channel — using only the onboard IR LED (transmitter) and the TSOP demodulating receiver. No extra hardware, cables, phones, computers, internet or radio is needed.

It is a rewrite of Flipper Share with the transport replaced by a custom IR modem. The basics of the classic flipper_share file-transfer protocol (resumable, integrity-checked) is preserved.

Actual file transfer speed via **Flipper Share IR** is around 130 Bytes/sec (7.6 KB/min), if you need to achieve higher throughput, consider using the original **Flipper Share** app over Sub-GHz ([README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ir/README.md)).

File size tested is 8 MB (~17 hours), but the protocol itself can support up to 4 GB (uint32_t file size). Maximum file size is limited by the Flipper Zero's free RAM on receiver side, actually should be not more than 35 MB.

Features:

- Works out of the box on any Flipper Zero — the IR hardware is built in.
- Integrity check with an MD5 hash after reception; per-packet CRC16.
- Automatic retransmission of lost/corrupted packets — the transfer continues "until success" (or until you restart it manually).
- Half-duplex link (the side that listens stays silent), matching the hardware.
- Torrent-like progress bar on the receiver; filename/size and ETA on the sender.
- Designed to avoid triggering nearby TVs / AV gear (the sync pulse and symbol timings do not match any common consumer IR protocol).
- No pairing needed; no encryption (anyone in the IR beam can receive — don't send sensitive data).
- Multi-receiver (broadcast) is not supported but actually works fine on small amount of receivers.

IR is directional and unregulated; aim the two units at each other, fairly close, for best results. Throughput is lower than radio (IR is a slow, half-duplex, line-of-sight link) but robust.

# Notes

Source code of the latest version is [here](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ir). Please feel free to open issues and PRs.

See the full [README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ir/README.md) for the IR modem and protocol description.

See the full [README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share/README.md) of the original Flipper Share app for more details and protocol description.

