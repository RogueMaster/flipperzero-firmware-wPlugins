# CLAUDE.md

Guidance for AI assistants (and humans) working in this repo.

## What this is

A Flipper Zero external app (FAP) that resets the filter-life counter on Xiaomi air
purifier NTAG213 filter tags. Written in C against the official firmware SDK.

## Architecture

- **`src/core/` — pure core, no firmware dependency.** `sha1.c` (bundled, public
  domain) and `xiaomi_filter.c` (password derivation, memory map, counter parsing).
  This is the crown jewel: it is unit-tested on the host, and the *same object code*
  is compiled into the FAP. Keep it free of `furi.h`/SDK includes.
- **`src/nfc/xiaomi_filter_worker.c` — the NFC state machine.** Drives a custom
  MfUltralight poller via `nfc_poller_start_ex` (extended mode). In the poller
  callback it: reads pages 0-3 for the UID → derives the password → `PWD_AUTH` →
  reads page 8 → writes zeros to page 8 → reads back to verify.
- **`scenes/` — UI.** ViewDispatcher + SceneManager with stock modules (submenu,
  popup, widget). Scenes: start, reset, success, error, about. Scene list is the
  X-macro in `scenes/xiaomi_filter_reset_scene_config.h`.
- **`xiaomi_filter_reset.c` / `_i.h` — app lifecycle and shared state.**

## Key domain facts (hardware-verified — see `docs/protocol.md`)

- Filter tag = **NTAG213** (NfcA). UID is 7 bytes.
- **Password = selected bytes of `SHA-1(UID)`**: `h[(h[0]+off)%20]` for
  `off ∈ {0,5,13,17}`. Verified against 5 real-tag golden vectors.
- **Usage counter = page 8**, little-endian `uint32`. Writing `00000000` = 100% life.
- Pages 4+ are password-protected for **read and write** (`AUTH0=4`, `PROT=1`), so you
  must `PWD_AUTH` before reading or writing the counter. UID (pages 0-1) is always
  readable.
- A successful auth is the definitive "genuine Xiaomi filter" signal. Never write
  without it.

## Build / test / lint

```sh
ufbt                    # build the FAP -> dist/xiaomi_filter_reset.fap
ufbt launch             # build + install onto a connected Flipper
ufbt lint               # clang-format check (formatter)
ufbt format             # auto-format all sources
make -C test test       # host unit tests (strict warnings act as a linter)
make -C test lint       # strict-warning compile of the pure core only
```

Target SDK: firmware **release** channel (developed against 1.3.4, API 86.0). CI also
builds the **dev** channel.

## Conventions

- **Conventional Commits** (`feat:`, `fix:`, `docs:`, `chore:`, `test:`, `ci:`).
- Documentation and code comments in **English**.
- Formatting is the firmware's `.clang-format` (committed). Run `ufbt format` before
  committing.
- Keep `src/core/` firmware-free and add a host test for any new logic there.

## Gotchas / notes

- **No hardware in the loop for CI.** Correctness of the NFC layer rests on: matching
  the official `mf_ultralight_poller_sync.c` idiom exactly (extended-mode poller,
  `event.poller` is the `MfUltralightPoller*`, `event.parent_event_data` is the
  `Iso14443_3aPollerEvent*`), and the FAP compiling clean against the real SDK.
  Behavioural changes to the NFC flow should be tested on a real device.
- The app icon (`icons/xiaomi_filter_reset.png`) is a 10x10 1-bit PNG required by the
  manifest; `fap_icon_assets` is intentionally unset (no in-app image assets).
- Catalog submission lives in `catalog/`; screenshots must be captured from a real
  device with qFlipper before publishing (`catalog/screenshots/` is a placeholder).
- The bundled SHA-1 is deliberate: it removes any dependency on the firmware exporting
  a hash, and guarantees the host tests exercise the shipping code.
