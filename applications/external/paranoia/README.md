# Paranoia Mode

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-0.2-blue)]()

Anti-surveillance field tool for Flipper Zero.  
Scans for hidden wireless cameras, RFID/NFC skimmers, and infrared monitoring devices.

---

## What Changed in v0.2 (Thanks to OiScout/Claude)

### Bug Fixes

| # | Location | Problem | Fix |
|---|----------|---------|-----|
| 1 | `paranoia_scan_nfc()` | Called `furi_hal_nfc_init()` — this is called once at boot by the firmware; calling it again corrupts NFC hardware state | Removed; now uses `furi_hal_nfc_acquire()` / `furi_hal_nfc_release()` (modern API) |
| 2 | `paranoia_scan_nfc()` | Called `furi_hal_nfc_event_stop()` without `furi_hal_nfc_release()`, leaving the NFC hardware exclusively locked | Added `furi_hal_nfc_field_detect_stop()` → `furi_hal_nfc_low_power_mode_start()` → `furi_hal_nfc_release()` |
| 3 | `paranoia_scan_rf()` | Threshold `(10 - sensitivity*3)` equals **0** at High — making `rf_signal_count >= 0` always true (always alerts) | Replaced with lookup table: `{3, 2, 1}` for Low/Med/High |
| 4 | `paranoia_scan_nfc()` | Threshold `(3 - sensitivity + 1)` equals **4/3/2** — *higher* sensitivity produced a *harder* threshold to trigger, inverted behaviour | Replaced with lookup table: `{3, 2, 1}` for Low/Med/High |
| 5 | `paranoia_scan_ir()` | `furi_hal_infrared_is_busy()` reflects **TX** activity, not incoming RX signal presence — always returned false during passive scan | Replaced with direct GPIO read of `gpio_infrared_rx` (PA7, active-low) to detect incoming IR bursts |
| 6 | `paranoia_scan_ir()` | Double-call to `furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL)` before and after the poll loop | Removed the pre-loop call (had no effect); kept single post-loop clear |
| 7 | `paranoia_draw_info_menu()` | `canvas_set_font(FontPrimary)` immediately overwritten by `canvas_set_font(FontSecondary)` with no draw call between | Removed dead first `set_font` call |
| 8 | `paranoia_draw_options_menu()` | Same dead `canvas_set_font(FontPrimary)` | Removed |
| 9 | `application.fam` | `requires=[...]` is not a valid fbt/ufbt FAM key — silently ignored but warns | Removed |

### Improvements

- **RF scan now probes 3 frequencies** (433.92 MHz, 315 MHz, 868.35 MHz) instead of only 433.92 MHz, improving hidden-camera detection coverage across US and EU market devices.
- **RSSI thresholds** are now a proper lookup table (`-70 / -80 / -90 dBm`) so High sensitivity captures weaker signals.
- **Threat level is clamped** inside each scan function (not only in the state machine) to prevent transient values above 10.
- **Threat bar**: a graphical fill bar is drawn alongside the numeric threat level.
- **Stack size** raised from 2 KiB to 4 KiB to accommodate Momentum's slightly larger interrupt frames and the NFC acquire/release call depth.
- **"None" label** shown in the detection row when no anomalies are found.

---

## Features

- **RF Scanning** — Detect hidden wireless cameras / transmitters on 433.92, 315, and 868.35 MHz
- **NFC Scanning** — Find RFID skimmers and unauthorised NFC readers
- **IR Scanning** — Identify infrared surveillance equipment passively via GPIO
- **Adjustable Sensitivity** — Low / Medium / High thresholds
- **Real-time Alerts** — Visual bar + haptic notification on detection
- **Threat Level** — 0–10 score with graphical progress bar

---

## Controls

| Button | Action |
|--------|--------|
| **Center** | Start / Stop scan |
| **Left** | Options menu |
| **Right** | Info screen |
| **Up** | Return to Idle |
| **Back** | Exit app |

---

## How It Works

### RF Detection
Tunes the CC1101 SubGHz module to each target frequency, starts async RX for 150 ms, samples RSSI 5 times per frequency, and counts readings above the sensitivity-adjusted threshold.

### NFC Detection
Acquires exclusive NFC hardware access, enables field-detection mode (passive — Flipper generates no carrier), and polls `furi_hal_nfc_field_is_present()` over 500 ms. A reading indicates a nearby NFC reader is energising the field.

### IR Detection
Samples the IR RX GPIO line (PA7, active-low) over 10 × 100 ms windows. A LOW state during any sample indicates a modulated IR burst is present.

### Threat Score
Each positive module adds `1 + sensitivity` (RF/NFC) or `2 + sensitivity` (IR) to the score. Score is clamped to 10. A haptic/audio alert fires on each anomaly detected.

---

## Limitations

- False positives are possible in high-RF or high-IR environments.
- NFC detection requires a reader actively energising the field; passive tags/cards will not be detected.
- IR range is limited by the Flipper Zero's on-board IR receiver sensitivity.
- Not a substitute for professional RF sweep equipment.

---

## License

MIT — see [LICENSE](LICENSE)

## Credits

- OiScout's app: [OiScout](https://github.com/OiScout/flipper-paranoia)
