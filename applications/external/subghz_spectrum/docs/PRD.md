# PRD: Sub-GHz Spectrum Analyzer (`subghz_spectrum`)

## Overview
A real-time RF spectrum analyzer for the Flipper Zero's CC1101 Sub-GHz radio.
Sweeps across a configurable frequency range, sampling RSSI at each step, and
renders a live scrolling waterfall / bar-graph on the 128×64 display.

## Problem Statement
The stock Flipper Zero "Frequency Analyzer" only reports a single peak frequency.
Pentesters performing RF reconnaissance need to see the **full RF landscape** —
which channels are active, how strong signals are, whether jamming is present, and
where rogue transmitters are operating. Today this requires a $300+ SDR + laptop.

## Target Users
- Physical-security pentesters performing RF site surveys
- Red teamers identifying wireless systems before engagement
- Hardware security researchers characterizing unknown RF devices

## Hardware Utilized
- **CC1101** transceiver via `furi_hal_subghz` API
- 128×64 monochrome LCD via `canvas` API
- 5-button D-pad + Back/OK for navigation

## Core Features

### F1 — Frequency Band Selection
| Band       | Start (MHz) | End (MHz) | Default Step (kHz) |
|------------|-------------|-----------|---------------------|
| 315 ISM    | 310         | 320       | 50                  |
| 433 ISM    | 425         | 445       | 50                  |
| 868 ISM    | 860         | 880       | 50                  |
| 915 ISM    | 900         | 930       | 50                  |
| Custom     | User-set    | User-set  | User-set            |

User can pick a preset band or enter custom start/end/step via number input.

### F2 — Real-Time Bar Graph View
- X-axis: frequency bins (one pixel column per bin, up to 128 bins)
- Y-axis: RSSI in dBm, mapped to pixel height (0-64)
- Refresh rate: as fast as sweep allows (~5-15 Hz depending on bin count)
- Peak-hold mode: shows decaying peak markers above live bars
- Current peak frequency + RSSI shown in top status line

### F3 — Waterfall View
- Same X-axis as bar graph
- Y-axis: time (newest at bottom, scrolls upward)
- Each row = one sweep, pixel brightness = RSSI intensity (dithered 1-bit)
- Configurable scroll speed

### F4 — Marker / Cursor
- D-pad left/right moves a vertical cursor across frequency bins
- Status line shows exact frequency + current RSSI at cursor position
- OK button "bookmarks" a frequency (stores to list, max 8)

### F5 — Data Logging
- Toggle logging to SD card: `spectrum_YYYYMMDD_HHMMSS.csv`
- CSV columns: `timestamp_ms, frequency_hz, rssi_dbm`
- One row per bin per sweep

### F6 — Settings
- Sweep step size: 10 / 25 / 50 / 100 / 200 kHz
- View mode: Bar Graph / Waterfall
- Peak hold: On / Off / Decay rate
- RSSI floor threshold (hide noise below N dBm)

## Non-Functional Requirements
- Must not hold Sub-GHz radio for more than 1 second continuously without yielding
- Must release radio cleanly on back-button exit
- Memory usage < 20 KB heap (waterfall buffer is the main consumer)
- All SD card writes buffered to avoid UI stutter

## Architecture
```
main ──► ViewDispatcher
           ├── spectrum_view (custom View, draws bar/waterfall)
           ├── settings_view (VariableItemList)
           └── band_select_view (Submenu)

spectrum_worker (FuriThread)
  └── loops: set_frequency → rx → get_rssi → idle → next_freq
      pushes RSSI array to spectrum_view via FuriMessageQueue
```

## Key API Calls
```c
furi_hal_subghz_set_frequency(freq);
furi_hal_subghz_rx();
furi_hal_subghz_get_rssi();  // returns float dBm
furi_hal_subghz_idle();
```

## Out of Scope
- Demodulation / protocol decode (that's app #3)
- Transmit capability
- External CC1101 module support (could be added later)

## Success Criteria
- Can identify an active 433 MHz keyfob transmission from 5m away
- Sweep of 20 MHz band completes in < 2 seconds
- Waterfall clearly distinguishes intermittent vs continuous transmitters
- CSV export opens correctly in Excel / LibreOffice / Python
