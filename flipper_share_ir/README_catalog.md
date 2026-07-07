# IR Share — direct file transfer between Flippers over IR

## Overview

IR Share transfers any file directly from one Flipper Zero to another over the
**infrared** channel, using only the onboard IR LED (transmitter) and TSOP
demodulating receiver — no extra hardware, cables, phones, computers, internet or
radio needed. Point the two Flippers' top edges at each other.

It is a rewrite of Flipper Share with the transport replaced by a custom IR modem;
the resumable, integrity-checked transfer protocol is preserved.

Features:

- Works out of the box on any Flipper Zero — the IR hardware is built in.
- Integrity check with an MD5 hash after reception; per-packet CRC16.
- Automatic retransmission of lost/corrupted packets — the transfer continues
  until it succeeds (or you restart it manually).
- Half-duplex link (the listening side stays silent), matching the hardware.
- Torrent-like progress bar on the receiver; filename/size and ETA on the sender.
- Designed not to trigger nearby TVs / AV gear (the sync pulse and symbol timings
  do not match common consumer IR protocols).
- No pairing; no encryption — anyone in the IR beam can receive, so don't send
  sensitive data.

IR is directional, line-of-sight and unregulated: aim the units at each other,
fairly close. Throughput is lower than radio but robust.

# Notes

See the full [README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ir/README.md)
for the IR modem and protocol description.

Source code of the latest version is
[here](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ir). Please feel
free to open issues and PRs.

# Credits

Derived from Flipper Share. The IR modem and transport are built on the Flipper
firmware `furi_hal_infrared` raw TX/RX API.
