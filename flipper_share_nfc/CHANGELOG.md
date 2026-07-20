v1.5: Flipper Share NFC — file transfer over the NFC channel
- New app derived from Flipper Share: the transport is now NFC (onboard 13.56 MHz
  antenna, ISO14443-3A) instead of Sub-GHz / IR.
- Role mapping: the sender emulates a card (NFC listener), the receiver acts as the
  reader (NFC poller) and drives a command/response exchange loop; each exchange
  carries one flipper-share packet per direction (or an empty keep-alive poll).
- DATA block size raised to 240 bytes to fill one ISO14443 frame (256-byte buffer),
  for much higher throughput than the IR build.
- Per-packet CRC16 kept (on top of the per-frame ISO14443 CRC-A); MD5 verification
  of the whole file after reception is kept.
- Automatic retransmission of lost/corrupted packets and resume after field loss:
  the poller re-activates the card and the block bitmap re-requests only the
  missing blocks.
- Builds with ufbt against the official firmware (no firmware modification); all
  NFC access goes through the official external app API.
- Transport parameters (emulated card identity, poll pacing, frame wait time,
  mailbox depth) live in nfc_transport_config.h; DATA packet size in nfc_share.h.
