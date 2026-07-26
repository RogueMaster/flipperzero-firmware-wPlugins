# Flipper Share RFID — direct file transfer between Flippers over the 125 kHz coil

## Overview

**Flipper Share RFID** transfers a file from one Flipper Zero to another over the
built-in **125 kHz LF RFID coil** — no extra hardware, cables, phone, computer, internet
or radio needed. One Flipper drives the field and reads (receiver); the other
load-modulates the field like a tag (sender).

It is a rewrite of Flipper Share with the transport replaced by a custom LF modem. The
basics of the classic flipper_share file-transfer protocol (resumable, integrity-checked)
are preserved.

> ### ⚠️ Hold the coils ~2 cm apart — do NOT press them together
>
> The link is very distance-sensitive. Pressed flat together the coupling is **too
> strong**: the sender's coil detunes the receiver's resonant tank (pulls the carrier off
> 125 kHz) and the demodulator loses the modulation, so nothing decodes while they sit
> still. Held **steady at about 2 cm** (a couple of millimetres of spacer — a few stacked
> cards work) the coupling is in the sweet spot and the transfer runs to completion.
> Keep them still at that gap; moving them around only flickers the link.

Actual transfer speed is around **315 B/s** (measured on 1 KB and 32 KB transfers; 32 KB
in ~1:44). This is a slow, alignment-sensitive, one-way-on-the-air link — for real
throughput use the original **Flipper Share** over Sub-GHz, or Flipper Share NFC / UART.

Features:

- Works out of the box on any Flipper Zero — the RFID hardware is built in.
- Integrity check with an MD5 hash after reception; per-packet CRC16.
- Resumes automatically: separate and re-align the coils mid-transfer and the receiver's
  block bitmap picks up where it left off.
- One-way on the air (carousel): the sender broadcasts continuously, the receiver never
  transmits — so there is no pairing and no handshake.
- Torrent-like progress bar and ETA on the receiver; filename/size and ETA on the sender.
- No encryption (anyone with a reader in range could receive — don't send sensitive data).

# Usage

1. Put a ~2 mm non-metallic spacer between the two Flippers' coil areas (back of the case),
   or plan to hold them at ~2 cm.
2. On the receiving Flipper: open Flipper Share RFID → **Receive**.
3. On the sending Flipper: open Flipper Share RFID → **Send** → pick a file → **OK**.
4. Bring the backs together to ~2 cm and **hold still**. The receiver locks on, shows a
   progress bar, and verifies the MD5 at the end; the file is saved to `/ext/inbox/`.

The sender shows the file name, size and a rough ETA. The receiver shows
"Waiting for announce..." until it
locks, then the progress bar with percentage and ETA.

---

# Flipper Share RFID protocol

Two layers: a **Manchester-ASK LF modem** (physical/link layer) under the existing
**file-transfer protocol**, run in a one-way **carousel** mode.

## Physical layer — the LF modem (`rfid_modem.*`)

- **Carrier:** the receiver drives 125 kHz at 50% duty (`furi_hal_rfid_tim_read_*`). The
  sender load-modulates it with double-buffered DMA (`furi_hal_rfid_tim_emulate_dma_*`);
  its modulation timer is clocked by the received field, so the sender only transmits
  while the receiver's field is present.
- **Line code:** Manchester at **RF/32** — one bit = 32 carrier cycles = 256 µs, each
  half-bit = 16 cycles = 128 µs. ASK (load on / off). Bits are LSB-first.
- **Framing:** `[preamble: 16 ones][sync 0x7E][len][packet bytes]`. Frames are separated
  by a short unmodulated gap; a corrupted frame just fails and the next preamble re-syncs.
  No modem CRC — the packet's own CRC16 and the final MD5 do the filtering.
- **Receiver decode.** The capture HAL reports PWM-style timing — high-pulse width on each
  falling edge and full period on each rising edge — which the transport turns back into
  constant-level run lengths (low run = period − high). Runs are classified as half-bit or
  full-bit at a midpoint threshold (no dead zone), the half-bit stream is rebuilt, and a
  sliding window hunts the preamble+sync in either polarity. A **debounce filter** absorbs
  brief comparator-noise edges that would otherwise split a real run — the key to decoding
  on the noisy, weakly-coupled coil link.

## Carousel mode (one-way on the air)

v1 has no uplink from the receiver, so the classic ANNOUNCE→REQUEST→DATA exchange is
replaced by a broadcast carousel (guarded by `#ifdef FSH_CAROUSEL` in the shared engine):

- **Sender** streams continuously: every 4th frame is an ANNOUNCE (file name, size, MD5),
  the rest are DATA blocks in round-robin order. Frequent ANNOUNCEs let a receiver lock on
  quickly even when the per-frame decode rate is low.
- **Receiver** never transmits. It locks to the first valid ANNOUNCE's `tx_id`,
  preallocates the file, fills the block bitmap as DATA arrives (duplicates ignored), and
  when all blocks are in computes the MD5 and compares it to the announced hash.
- Lost or corrupted (CRC16-failing) frames are simply re-delivered by the carousel next
  pass, so the transfer converges.

## Packet structure

Every packet: `[version(1)][tx_id(1)][packet_type(1)][payload][crc16(2)]`. The payload
length depends on the type.

### `0x01` — Announce (control payload)

| Field       | Size     | Type                  |
|-------------|----------|-----------------------|
| `file_name` | 36 bytes | char[36], zero-padded |
| `file_size` | 4 bytes  | uint32_t              |
| `file_hash` | 16 bytes | MD5                   |

### `0x03` — Data (data payload)

| Field        | Size            | Type     |
|--------------|-----------------|----------|
| `block_num`  | 4 bytes         | uint32_t |
| `block_data` | FSH_DATA_LENGTH | raw data |

(The `0x02` Request packet exists in the shared format but is unused in carousel mode.)

## Files

- `share.c` / `share.h` — shared file-transfer engine (byte-identical across the new
  Flipper Share apps), with the `#ifdef FSH_CAROUSEL` blocks for this app.
- `rfid_modem.c/.h`, `rfid_modem_config.h` — pure-C Manchester-ASK codec, host-testable.
- `rfid_transport.c/.h` — HAL glue: the sender's DMA refill, the receiver's capture feed,
  decode and delivery.
- `share_config.h` — all tunables (RF/32, timings, carousel interval, throughput).
- `tools/modem_test.c` — host test harness (`cc tools/modem_test.c rfid_modem.c`).

# Credits

Derived from Flipper Share. LF modem and transport built on the Flipper firmware
`furi_hal_rfid` raw reader/emulate API.
