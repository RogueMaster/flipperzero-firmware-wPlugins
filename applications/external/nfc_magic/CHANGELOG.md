# Changelog

## 2.4

### Added

- **ISO15693 / NfcV support** - The app can now detect an ISO15693 tag, show its Info, and
  **clone / wipe / write the UID** of a magic ISO15693 card the same way it handles other magic
  types. **gen1 and gen2 cards only; gen3 is not supported.**
  - Magic status on ISO15693 can only be confirmed by writing, so the scan shows every ISO15693
    tag as a Magic candidate.
  - **A wipe can brick a gen3 card.** It cannot tell if a card is gen3, and zeroing an
    un-finalized gen3 card's configuration bricks it permanently, per the author of proxmark's
    ISO15693 V3 support (not tried in testing). The wipe's confirmation screen warns about it.
  - **A wipe can change a gen1 card's UID**, since blocks 56/57 are its UID registers. The app
    re-reads the UID afterwards and, where it can, displays the UID the card now answers to.
  - **Keep one tag in the field at a time.** Most writes are addressed to the card, but the gen2
    UID write and all reads are not.
  - See [ISO15693.md](ISO15693.md) for how each operation behaves and what it was tested on.

## 2.3

### Added

- **Gen 4 cards whose configuration dump is 30 or 31 bytes are now detected.** The dump length is
  firmware specific: alongside the 32-byte variant the app knew, Ultimate Magic Cards exist that
  answer the backdoor config command with 30 or 31 bytes, and Proxmark3 has accepted 30 or 32
  since 2022. Anything other than 32 was rejected as **not a magic card**, which also made the
  card unwritable, because writing is only offered once detection succeeds. All three lengths are
  now accepted, the length the card returned is remembered, and the configuration is written back
  at that same length rather than always 32. A dump of any other length is still rejected, but it
  is now logged with its byte count so an unknown variant can be told apart from a card that
  simply is not a Gen 4. The Gen 4 info screen reports the length it read.

### Fixed

- **The configuration write no longer sends uninitialized memory to the card.** The write path
  filled 28 bytes of a 32-byte configuration buffer from its template and the remaining documented
  fields by hand, but the two undocumented trailing bytes were never assigned, and the poller
  allocates its state without zeroing it. Those two bytes were transmitted and stored as part of
  the card configuration on every Gen 4 write, differing between runs. The card's own trailing
  bytes are now read back and carried over unchanged, and zeroed if they cannot be read.

- **The Gen 4 poller no longer dispatches on an uninitialized state.** Its state field was never
  set to the idle state at allocation, so the first card callback indexed the state handler table
  with whatever the heap held. It worked while the allocator happened to return zeroed memory,
  which stops being true the second time a Gen 4 screen is opened in one session.

## 2.2

### Changed

- **Gen2 detection now tries the per-UID key cache for sector 0.** Confirming a CUID means
  authenticating to sector 0 before block 0 can be probed, and the probe only ever knew the
  default FF..FF key, so a clone with a personalised sector 0 and no recognised ATS came back as
  **Magic Not Confirmed**. The sector-0 key A and key B the NFC app recorded for that UID in
  `/ext/nfc/.cache/<UID>.keys` are now offered first, ahead of FF..FF, so such a card can be
  confirmed as Gen 2 / CUID and still gets its static-nonce classification. The cache is read once
  per card per scan, a card with no entry behaves exactly as before, and a cached key that doesn't
  fit costs one more RF session and nothing else: the probe itself is unchanged, still only the
  first phase of the write, so block 0 is never modified.

## 2.1

### Added

- **MIFARE Classic key cache phase** - the dictionary attack now tries the NFC app's per-UID key
  cache (`/ext/nfc/.cache/<UID>.keys`) before the user and system dictionaries, for both **Write**
  and **Wipe**. A magic clone carries the original card's UID, so keys the NFC app recovered when
  the original was saved are already on the SD card under the clone's own name. They are fed to
  the poller as a dictionary, so each one is still authenticated against the card in front of you;
  a stale entry costs a few failed auths and nothing more. When the cache alone finishes the card,
  the two dictionary phases are skipped instead of being run for nothing. A card with no cache
  entry runs exactly as before.

### Fixed

- **Wiping a Gen2 clone of a static-encrypted-nonce card (FM11RF08S)** dead-ended at
  **"No keys found"**. Those keys only ever reach the per-UID dictionary, so neither shared
  dictionary has them; the key cache phase now does.

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
