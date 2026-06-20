# Repo notes (agent/human)

This repository contains a Flipper Zero external app for controlling an FM receiver and (optionally) an external I2C volume controller.

## What it is
- App entry: [application.fam](application.fam)
- Main app/UI logic: [radio.c](radio.c)
- TEA5767 driver: [TEA5767/](TEA5767/)
- PT audio driver support: [PT/](PT/)

## Build / run
This is an external app. Typical workflow:

- Build: `ufbt build`
- Install + launch (device connected): `ufbt launch`

## Virtualenv (.venv) note
This repo contains a Python virtual environment at `.venv/`. In some setups `ufbt` is installed inside that venv,
so opening a fresh terminal without activating `.venv` may lead to `ufbt: command not found`.

- Activate venv for this repo: `source .venv/bin/activate`
- Verify tools: `command -v ufbt && ufbt --version`

If you prefer `ufbt` to work regardless of venv activation, install it globally via `pipx install ufbt`.

## Persistent data on device
Settings and presets are stored on the SD card:

- `/ext/apps_data/fmradio_controller_pt2257/settings.fff`
- `/ext/apps_data/fmradio_controller_pt2257/presets.fff`

## Publishing / licensing
- License: GPLv3 (see [LICENSE](LICENSE)).
- Upstream origin is documented in [README.md](README.md).
- If you publish built binaries (e.g. `.fap`) from modified sources, publish the corresponding source code for that exact version (tag/commit) and keep attribution.

## Audit / refactor plan (2026-02-21)

Status legend: `[ ]` = do zrobienia, `[x]` = gotowe, `[~]` = w trakcie

### A. Martwy kod / nieużywane zmienne
- [x] A1: Usunąć `int* signal_strength;` (radio.c) — nieużywany
- [x] A2: Usunąć `uint8_t tea5767_registers[5];` (radio.c) — nieużywany
- [x] A3: Uprościć `volume_values[]` / `volume_names[]` (radio.c)
- [x] A4: j.w. (inline volume_names)
- [x] A5: Usunąć `char* current_vol` (radio.c) — ustawiany, nigdy odczytywany
- [x] A6: Usunąć `struct StationInfo` (TEA5767.c) — nieużywany duplikat
- [x] A7: Usunąć `RADIO_BAND` enum (TEA5767.h) — nieużywany

### B. Logika / poprawność
- [x] B1: Poprawić wcięcie w `tea5767_write_registers` (TEA5767.c)
- [x] B2: Usunąć redundancję `REG_3_SSL | 0x60` → `REG_3_SSL` (TEA5767.c)
- [x] B3: Usunąć `tea5767_MuteOff()` (niebezpieczna read-modify-write)
- [x] B4: `bool current_volume = false` zamiast `= 0` (radio.c)

### C. Nieużywane funkcje TEA5767
- [x] C1–C8: Usunięte: seekUp, seekDown, ToggleMute, MuteOn, MuteOff, SetFreqKHz, set_mute, set_stereo (+ deklaracje z .h)

### D. Refaktoring / uproszczenia
- [x] D1: Rename `FlipTheWorld` → `Listen`
- [x] D2: Uprościć/usunąć martwy `MyModel` struct
- [x] D3: Wynieść logikę I2C polling / save / scan z draw_callback do FuriTimer
- [x] D4: Usunąć nieaktualne komentarze w elements_button_top_*

### E. Styl / drobnostki
- [x] E1: Usunąć pustą linię na początku radio.c
- [x] E2: Dodać `static` do globalnych zmiennych (radio.c)
- [x] E3: Poprawić literówkę "SLC" → "SCL"
