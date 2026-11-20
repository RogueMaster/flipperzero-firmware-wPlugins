# Changelog

## 1.6

- Fix NFC key sharing, which did not work at all after the 1.4 rewrite: the key read failed on the very first page of the card every time
- Emulate the shared card with the firmware's own card generator instead of filling in an NTAG215 by hand, so every fixed page, the version bytes, the counters and the configuration pages are what the NFC stack expects. The hand-built card left the UID and its BCC bytes, the capability container and `AUTH0` unset, and generated a UID without the constraints real silicon has
- Set the card's page count explicitly on both sides rather than letting it ride along on `mf_ultralight_copy()`, which does not carry `pages_read` and `pages_total` on every firmware revision. A card that claimed no pages was refused on every page read, while `GET_VERSION`, `READ_SIG`, `READ_CNT` and `CHECK_TEARING` still answered, which is what made this look like a protocol fault rather than an empty card
- Read the payload straight out of the poller's page array, so the reading side no longer depends on that bookkeeping either
- Move the payload into user memory at page 4. Pages 0 to 3 hold the UID, its BCC bytes and the capability container on a real card, and the old layout put key material there. This changes the on-air format, so a key can no longer be shared with an app older than 1.6, correcting what the 1.4 entry below claimed
- State the payload length and an `"ESGC"` marker in the card instead of implying the length by where the readable pages end, so an unrelated NTAG215 or a card from an older app version is rejected with a clear message rather than parsed as key material

## 1.5

- Fix the launch crash on the Official firmware: the app-local copies of the firmware's private GUI structs were Unleashed-only, so on Official `bool was_drawn` put `draw_callback` at offset 16 instead of 12 and the hooked draw callback jumped to a garbage pointer, the same fault that 1.3 fixed for Unleashed
- Split `struct ViewPort` and `struct ViewDispatcher` on `FW_ORIGIN_Official`, verified field by field against both firmware.elf DWARF layouts (Unleashed dev 000b238e, Official 1.5.1)
- Add `_Static_assert`s on the offsets this app actually reads, so an edit to the copied structs fails the build instead of the device

## 1.4

- Drop the bundled `lib/nfclegacy` compatibility library: the app no longer ships its own copy of the pre-rework NFC stack (ST25RFAL002 driver, `furi_hal_nfc`, `nfc_worker` and the MIFARE Ultralight protocol code) and no longer drives the ST25R3916 behind the firmware's back
- Rewrite NFC key sharing on top of the current firmware API: reading uses `NfcPoller` with the MIFARE Ultralight poller, sharing uses `NfcListener`
- Claim the NFC chip only while the key read or key share popup is on screen instead of holding it for the whole app lifetime, so other NFC apps keep working while the chat runs in the background
- The emulated NTAG215 layout is unchanged, so key sharing still works against Flippers running an older version of the app

## 1.3

- Fix the MemManage crash on launch: the app's local copies of the firmware's private GUI structs had drifted from the firmware layout, so the view port's original draw callback was read from the wrong offset and the hooked draw callback jumped to a garbage pointer on the first frame
- Re-sync `struct ViewPort`, `struct ViewDispatcher` and `struct Gui` with the firmware (FAP API 88.25), restoring the keyboard lock overlay and the input hooks that the chat view depends on
- Document in the copied headers that they must stay byte-identical to the firmware, since the FAP API version check cannot detect this class of drift

## 1.2

- Version bump for catalog compatibility (no functional changes)

## 1.1

- Initial versioned release: reimplements the Sub-GHz CLI chat as an app, with optional message encryption keyed by password, hex input, a generated random key or a key read from another Flipper over NFC, a configurable frequency, key display and NFC key sharing, and a lockable keyboard in the chat view
