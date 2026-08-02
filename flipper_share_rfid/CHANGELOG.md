v0.1: EXPERIMENTAL: Flipper Share RFID — file transfer over the 125 kHz coil
- New app derived from Flipper Share: the transport is a custom Manchester-ASK LF modem (RF/32) built on the raw `furi_hal_rfid` reader/emulate API.
- One-way carousel mode: the sender broadcasts ANNOUNCE + DATA frames continuously, the receiver never transmits — no pairing, no handshake.
- Resumable: block bitmap picks up where the link dropped; per-packet CRC16 and a whole-file MD5 check after reception.
- Coupling-sensitive by nature: hold the coils ~2 cm apart (not pressed together) and keep them still; ~315 B/s measured.
