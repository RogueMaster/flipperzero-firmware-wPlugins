# Changelog

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
