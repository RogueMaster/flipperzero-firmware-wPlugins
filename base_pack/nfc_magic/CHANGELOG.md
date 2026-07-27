# Changelog

## 2.1

Adds magic **ISO15693 / NfcV** support. Detect an ISO15693 tag, show its Info, and
**clone / wipe** a magic ISO15693 card the same way the app handles its other magic types.

The magic write frames are an exact port of proxmark3's `SetTag15693Uid` (gen1) and
`SetTag15693Uid_v2` (gen2).

### Added
- **Detection** — any ISO15693 tag that activates is treated as a magic candidate and routed to a
  dedicated menu (Write / Wipe / Write UID / Info), mirroring the other magic types.
- **Info** — UID, manufacturer, chip type, GET SYSTEM INFO (memory / DSFID / AFI / IC ref), and the
  full block data (scrollable, `*` marks a locked block). Chip decode tells NXP **SLI / SLIX / SLIX2**
  (and the -S / -L variants) apart via the UID type-indicator bits.
- **Clone from a saved `.nfc`** — writes the UID (magic backdoor), all data blocks, and the source's
  identity (IC ref / block geometry / AFI / DSFID) so the copy advertises the same chip. gen2 sets
  UID + geometry via the `0xE0` magic command; a gen1 card falls back to the block-write backdoor.
- **Wipe** — zero every writable data block; the UID is left unchanged.
- **Write UID** — manual magic backdoor UID write with a confirmation screen.

### Behaviour
- **Writes every source block and reports only real data loss.** WRITE BLOCK on these cards is gated
  by physical memory, not the advertised block count (verified on hardware), so the clone attempts
  every block: a non-empty block that won't write is reported as **Partial** (named); an empty block
  past the card's real capacity loses nothing, so it stays a **Success** — but one flagged with a
  note that the card now advertises more blocks than it physically holds (a reader probing the top
  blocks sees them error/zero, and real data can't be stored there). A card that advertises a larger
  geometry than it physically holds (fake-flash) clones faithfully for the blocks that fit.
- **gen1 fidelity is surfaced.** The gen1 backdoor stores the UID in data blocks 56/57/62/63, so a
  gen1 clone can't reproduce a source that uses them. If the source has data there, the confirm warns
  before the write; if the clone actually fell back to gen1, it reports the Partial clone status and
  flags those blocks.
- **Verifies after an RF field power-cycle** (`NfcCommandReset`), so a card that only latches the new
  UID after a reset is not misreported as a failure.
- The potentially destructive **gen1 fallback only runs if the gen2 write left the UID unchanged**,
  so a gen2 card is never clobbered by gen1.
- Non-magic / removed-card outcomes show dedicated **"Not a magic tag"** / **"Card removed"** messages
  instead of a generic error, and detect / write popups **time out** instead of hanging.

### Validation
- The **gen2** path is validated end-to-end on hardware: byte-identical clones across chip types and
  geometries (28 / 56 / 64 / 70 blocks), confirmed by both the stock NFC read-back and a Proxmark3
  cross-read; wipe and the honest over-capacity reporting confirmed too.
- The **gen1** path is a faithful proxmark port but is **not yet hardware-validated** (no gen1 magic
  ISO15693 card was available to test against).

## 2.0

Major release. Adds magic **Ultralight / NTAG (USCUID-UL)** support, and reworks the magic
**MIFARE Classic (Gen2)** wipe & clone with honest **Success / Partial / Fail** reporting.

### Added

- **Magic Ultralight / NTAG (USCUID-UL) support** — the app can now write, clone and wipe magic
  Ultralight-family tags, not just MIFARE Classic:
  - **Write / full clone** with the transport auto-selected from detection — **direct** (CUID/ATS,
    ISO14443-3 + `A2`) or **backdoor** (raw wakeup). ACK-only, continue-on-fail; the UID page is
    written last; live "Writing X/N" progress; **Partial Write** with a Details list of the pages
    that didn't take (and resume if the card is briefly removed mid-write).
  - **Wipe** to factory default.
  - **Auth with Password (PWD-AUTH)** to write protected/locked tags (single attempt, so it can't
    burn `AUTHLIM`).
  - **"Write anyway"** for tags that don't confirm as magic.
  - **Detection** of UL11 / UL21 (incl. Mikron "Ultra"), NTAG213/215/216, UL-C, UL-5, and
    backdoor-mode tags (both wakeup sequences), with a raw-config view for unrecognised presets;
    family-first scanner.
- **Gen2 / MFC wipe & clone now report Success / Partial / Fail** — a **Partial** screen plus a
  **Details** list of the exact blocks that couldn't be written/wiped (it was previously always
  "Success").
- **"No keys found"** screen when a wipe has no usable keys (instead of a doomed write-check).
- **Exit → menu** button on the Wipe / Write / Dump failure screens.

### Changed

- Gen2 / MFC wipe & clone outcomes are tracked **per block** (counts + the not-done block list).
- A wiped **block 0** preserves the card's real **SAK/ATQA** (a 1K mis-detected as 4K is no longer
  stamped 4K) and zeroes the UID.
- **Write-check warnings** reworked: one shared Gen2/Classic handler and consistent
  **Back / Next / Skip** buttons.
- Clearer rejection message for a cross-family wrong dump.

### Fixed

- **False "Success"** on a Gen2/MFC **wipe** with missing keys, and on a **clone** that could write
  nothing (unknown keys, or a read-only block 0).
- **Stale-key carry-over** between operations — the dictionary attack now re-reads the card fresh
  each time instead of replaying the previous tag's keys.
- **Per-sector → per-block** failure tracking — a read-only block 0 no longer reports the whole of
  sector 0 as failed.
- Dead **"Next"** button on a write-check warning whose first item wasn't the UID warning.
- Gen2 write-check **"Back"** did nothing (it targeted the wrong menu); the misleading **"Retry"**
  label → **"Back"** (the button always navigates back).
- **KeysDict** memory leak when backing out of a write-check.
- The wipe-failure screen could show **"No keys found"** for an unrelated failure (uninitialised
  scene state).

## 1.12

### Added

- **Gen2 CUID / ATS + static-nonce detection** — classify magic MIFARE Classic sub-types (direct
  CUID, ATS-fingerprinted, and CUID with a static nonce).
- **Gen1 4- and 7-byte UID** handling, including writing a 7-byte MIFARE Classic dump to a Gen1 tag.
- **Length-aware wipe & write guards** — validate block counts before writing.

### Changed

- Updated for new firmware API / SDK.

### Fixed

- Gen4 max block number for the NTAG protocol.

## 1.11

### Changed

- Description / metadata update.

### Fixed

- Gen4 poller fix.

## 1.10

- Upstream sync and maintenance.

## 1.9

### Changed

- Gen4 sync.

### Fixed

- Minor UI fix.

## 1.8

### Added

- **Gen4 (UMC / GTU) magic-card support.**

## 1.7

### Changed

- Reverted not-release-ready Gen2 changes (Gen4 still pending).

### Fixed

- UI and newline fixes.

## 1.6

### Changed

- Incremental updates; Gen2 preparation added, then reverted as not release-ready.

## 1.5

### Changed

- Reworked Back-button event handling; GUI cleanup.

### Fixed

- Incorrect total-block usage (#102); ufbt build compatibility.

## 1.4

- Maintenance / upstream sync.

## 1.3

### Fixed

- New-API compatibility fixes.

## 1.1

- Early release updates; new-API compatibility pass.

## 1.0

### Added

- Initial release in the plugin pack: write & wipe magic cards (**Gen1a**, **Gen2**) with
  block-0 / UID editing.
