# Flipper Share NFC — direct file transfer between Flippers over NFC

## Overview

**Flipper Share NFC** transfers any file directly from one Flipper Zero to another over the **NFC** channel (13.56 MHz, ISO14443-3A) — using only the onboard NFC antenna. No extra hardware, cables, phones, computers, internet or radio is needed.

It is a rewrite of Flipper Share with the transport replaced by an NFC poller/listener pair. The basics of the classic flipper_share file-transfer protocol (resumable, integrity-checked) are preserved.

Actual file transfer speed via **Flipper Share NFC** is around 7 KB/s — the fastest Flipper Share transport — but the two Flippers must be held together, back to back.

File size tested is 32 MB (~1 h 16 min), transfer time matches the ETA estimate. The protocol itself supports up to 4 GB (uint32_t file size). Maximum file size is limited by the Flipper Zero's free RAM on the receiver side (block map + progress state).

Other Flipper Share transports (Sub-GHz, IR & more): [github.com/lomalkin/flipper-zero-apps](https://github.com/lomalkin/flipper-zero-apps)

Features:

- Works out of the box on any Flipper Zero — the NFC hardware is built in. Builds with `ufbt` against the official firmware; no firmware modification.
- Integrity check with an MD5 hash after reception; per-packet CRC16 (on top of the ISO14443 CRC-A per frame).
- Automatic retransmission of lost/corrupted packets — the transfer continues "until success" (or until you restart it manually). Separating and re-touching the devices mid-transfer resumes where it left off.
- Half-duplex command/response link: the sender emulates a card (listener), the receiver acts as the reader (poller).
- Torrent-like progress bar on the receiver; filename/size and ETA on the sender.

NFC is near-field: hold the two Flippers with their NFC antennas back to back aligned and touching for best results. The working gap is only a few centimeters.

# Usage

1. On the receiving Flipper: open Flipper Share NFC → **Receive via NFC**.
2. On the sending Flipper: open Flipper Share NFC → **Send via NFC** → pick a file → **OK**.
3. Hold the two NFC antennas (top/back edge) together until it completes.
   The receiver shows a progress bar and verifies the MD5 hash at the end; the
   file is saved to `/ext/inbox/`.

---

# Flipper Share NFC protocol

The design is a two-layer stack: an NFC **transport** (physical/link layer) under
the existing **file-transfer protocol** (selective-repeat ARQ). The file-transfer
protocol is identical to the other Flipper Share builds; only the transport differs.

## Physical / link layer — the NFC transport

- Technology: **ISO14443-3A** at 106 kbit/s on a 13.56 MHz carrier. Activation and
  anticollision are handled by the firmware NFC stack; each ISO14443 frame is
  protected by the standard CRC-A (added/checked by the stack).
- **Role mapping:** the **sender** emulates a card (NFC *listener*) with a fixed
  UID/ATQA/SAK; the **receiver** is the reader (NFC *poller*) and drives a
  continuous command/response exchange loop.
- **Framing inside a frame:** `[transport_hdr(1)][flipper-share packet]`. The
  header is `0x01` when a packet follows, or `0x00` for an empty keep-alive poll
  (when a side has nothing queued). Each exchange carries at most one flipper-share
  packet in each direction.
- **Outbound mailbox:** the protocol's `cb_send_bytes` enqueues packets into a
  small queue; the poller drains one per command, the listener drains one per
  response. This bridges the protocol's fire-and-forget send with the synchronous
  NFC exchange, and provides natural backpressure for the DATA stream.
- **Resume:** field loss simply stalls the exchange loop; on every failed
  activation or exchange the poller cycles its RF field off (~100 ms) and
  retries. The field-off is essential, not just a retry pause: it is what
  resets the emulating side back to the answerable IDLE state if it was left
  mid-anticollision (in the ACTIVE state the emulated card ignores polling
  entirely, and only a field-off clears it). The protocol's block bitmap then
  re-requests only the missing blocks, so the transfer continues after
  re-touching the devices.
- Frame size is capped by the firmware's 256-byte NFC buffer; the DATA block size
  (`NSH_DATA_LENGTH`, default 240) is sized to fill one frame. All transport
  parameters live in `nfc_transport_config.h` (recompile to tune).
- **Field management:** the reader's RF field is only kept up while a peer is
  linked. While waiting for a sender (and after a link has been lost for more
  than a short grace window), the receiver duty-cycles the field: one ~6 ms
  detect probe every 500 ms (~1% duty) — the antenna stays cold and the battery
  drain is negligible, at the cost of a sub-second discovery latency. A transfer
  interrupted for minutes or hours therefore costs almost nothing while the
  devices are apart, and resumes automatically when the antennas re-touch. Once
  reception completes (Success / Hash failed screen) the field is switched off
  entirely.

## Packet structure

Every packet: `[version(1)][tx_id(1)][packet_type(1)][payload][crc16(2)]`.
The payload length depends on the type, so the DATA packet size is tunable and
decoupled from the control packet (which must hold the file metadata).

### `0x01` — Announce (control payload)

| Field       | Size     | Type                  |
|-------------|----------|-----------------------|
| `file_name` | 36 bytes | char[36], zero-padded |
| `file_size` | 4 bytes  | uint32_t              |
| `file_hash` | 16 bytes | MD5                   |

### `0x02` — Request range (control payload)

| Field     | Size    | Type     |
|-----------|---------|----------|
| `start`   | 4 bytes | uint32_t |
| `end`     | 4 bytes | uint32_t |
| padding   | rest    | zero     |

### `0x03` — Data (data payload)

| Field        | Size               | Type     |
|--------------|--------------------|----------|
| `block_num`  | 4 bytes            | uint32_t |
| `block_data` | NSH_DATA_LENGTH    | raw data |

## Session

- **Sender** announces the file (name, size, MD5) periodically until a receiver
  locks on, then streams the requested DATA blocks in its exchange responses.
- **Receiver** locks to the first valid announce (`tx_id`), preallocates the file,
  and requests the missing block range on timeout. It writes each block once
  (duplicates ignored) and, when all blocks are in, computes the MD5 and compares
  it to the announced hash.
- Lost or corrupted (CRC16-failing) packets are simply re-requested, so the
  transfer converges.

# Smartphone interoperability

A frequent question is whether this could transfer files to/from a phone over NFC
without a companion app. In short: **not usefully, without writing a phone app.**

- Stock Android NFC only reads NDEF tags and dispatches intents (open a URL, add a
  contact). The old file-push feature (Android Beam) was removed in Android 10 and
  replaced by Bluetooth/Wi-Fi based sharing. So the most a phone would do with no
  app is read a single NDEF message (a few KB: link / text / vCard) if the Flipper
  emulated an NFC Forum Type 4 tag — this is not a file transfer, and there is no
  phone→Flipper direction.
- iOS is stricter still: background NDEF reads only; anything else needs a custom
  app using CoreNFC, and card emulation is unavailable.
- With a companion app it is feasible: an Android reader-mode app (`NfcA.transceive`,
  ~253-byte frames — the same limit this transport uses) could speak this protocol
  directly to a Flipper listener for the Flipper→phone direction with no changes on
  the Flipper side. The phone→Flipper direction would need Host Card Emulation on
  the phone (ISO-DEP/APDU only) and an ISO14443-4 poller on the Flipper.

This build does not target phones, but the ISO14443-3A design does not preclude a
future companion app.

# Credits

Derived from Flipper Share. NFC transport built on the Flipper firmware NFC stack
(`nfc`, ISO14443-3A poller/listener), all through the official external app API.
