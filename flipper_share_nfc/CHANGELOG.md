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
- Reliable (re)connection: the poller cycles its RF field off on every failed
  activation attempt, resetting an emulator left stranded mid-anticollision (in
  that state the emulated card ignores polling and only a field-off recovers
  it — previously the link could stay dead until the devices were pulled far
  apart and re-touched).
- Control traffic (ANNOUNCE / REQUEST) now has priority over DATA in the transport
  mailbox (a separate latest-wins slot), so a DATA stream can no longer starve it;
  and the sender adopts the receiver's latest REQUEST even while mid-serving. Fixes
  a rare stuck transfer where the receiver could never (re)lock because ANNOUNCE
  packets were being dropped from a DATA-saturated queue.
- Builds with ufbt against the official firmware (no firmware modification); all
  NFC access goes through the official external app API.
- Transport parameters (emulated card identity, poll pacing, frame wait time,
  mailbox depth) live in nfc_transport_config.h; DATA packet size in nfc_share.h.
