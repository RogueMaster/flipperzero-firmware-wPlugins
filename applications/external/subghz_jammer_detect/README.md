# SubGHz Jammer Detector

```
 ____        _      ____  _
/ ___| _   _| |__  / ___|| |__  ____
\___ \| | | | '_ \| |  _ | '_ \|_  /
 ___) | |_| | |_) | |_| || | | |/ /
|____/ \__,_|_.__/ \____||_| |_/___|
     _                                    ____       _            _
    | | __ _ _ __ ___  _ __ ___   ___ _ _|  _ \  ___| |_ ___  ___| |_
 _  | |/ _` | '_ ` _ \| '_ ` _ \ / _ \ '__| | | |/ _ \ __/ _ \/ __| __|
| |_| | (_| | | | | | | | | | | |  __/ |  | |_| |  __/ ||  __/ (__| |_
 \___/ \__,_|_| |_| |_|_| |_| |_|\___|_|  |____/ \___|\__\___|\___|\__|
```

**Real-time Sub-GHz RF jamming detection across 4 frequency bands for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.x-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-Sub--GHz-red)](.)

> **For authorized RF security monitoring and research only.**

---

## What It Does

SubGHz Jammer Detector continuously monitors four common Sub-GHz frequency bands for suspicious RF activity that may indicate jamming. It uses the Flipper Zero's built-in CC1101 radio to measure RSSI (signal strength) at each frequency and classifies the activity level:

- **OK** — normal background noise
- **Suspicious** — elevated signal strength (possible interference)
- **Jammer** — strong sustained signal (likely intentional jamming)

Monitored frequencies:
| Frequency | Common Use |
|---|---|
| 315 MHz | North American vehicle keys, IoT sensors |
| 433.92 MHz | ISM band — garage openers, key fobs, weather sensors |
| 868.35 MHz | European ISM — Z-Wave EU, LoRa EU, alarm systems |
| 915 MHz | North American ISM — Z-Wave US, LoRa US, LPWAN |

Use cases:
- Detect jamming attacks against wireless alarm systems and key fobs
- Blue team monitoring — alert on RF interference during security events
- RF environment assessment — identify noisy frequencies before deploying wireless sensors
- Education — demonstrate RF jamming detection principles

---

## Installation

**Build:**
```bash
cd subghz_jammer
ufbt
```

**Deploy:**
```
dist/subghz_jammer.fap  ->  /ext/apps/Sub-GHz/subghz_jammer.fap
```

No external hardware required — uses the Flipper Zero's built-in CC1101 radio.

---

## Usage

### Main View

```
SubGhz Jammer Detect
------------------------------
315MHz   [========    ] -62  ok
433MHz   [============] -38  JAM
868MHz   [===         ] -78  ok
915MHz   [=========   ] -55  SUS
------------------------------
!! JAMMER 433MHz !!
```

Each row shows:
- Frequency label
- RSSI bar graph (normalized to display width)
- RSSI value in dBm
- Status: `ok`, `SUS` (suspicious), or `JAM` (jammer detected)

The bottom status line shows the worst current threat.

### Controls

| Button | Action |
|---|---|
| OK | Open Settings |
| Back | Exit app |

### Settings

| Setting | Options | Default |
|---|---|---|
| Suspicious RSSI | -70 / -60 / -50 dBm | -60 dBm |
| Jammer RSSI | -50 / -40 / -30 dBm | -40 dBm |
| Alert Mode | Silent / Blink / Vibrate | Blink |

---

## Detection Algorithm

1. CC1101 radio tunes to each of 4 frequencies in round-robin (OOK modulation, 650 kHz bandwidth)
2. Dwells 200ms per frequency to allow RSSI to settle
3. RSSI reading is pushed into an 8-sample rolling window per frequency
4. Rolling maximum is compared against thresholds:
   - >= Jammer threshold -> **Jammer** (after 3 consecutive scans)
   - >= Suspicious threshold -> **Suspicious** (after 3 consecutive scans)
   - Below both -> **OK** (counter resets)
5. The worst-case frequency determines the alert level
6. Alerts: Blink mode flashes red LED; Vibrate mode adds haptic feedback

Full scan cycle: ~800ms (4 frequencies x 200ms dwell).

---

## Architecture

```
subghz_jammer_worker.c (background thread)
  └── CC1101: tune -> dwell 200ms -> read RSSI -> detect -> alert
        |
        v (mutex-protected JammerState)
        |
subghz_jammer.c (300ms timer -> snapshot -> view model -> canvas)
```

| File | Purpose |
|---|---|
| `subghz_jammer.c` / `.h` | Main app, dual-view GUI (monitor + settings), refresh timer |
| `subghz_jammer_worker.c` / `.h` | CC1101 radio control, RSSI measurement, detection logic, alerts |

---

## Limitations

- **Sequential scanning** — monitors one frequency at a time (CC1101 is single-channel). A sophisticated jammer could hop between frequencies.
- **OOK wideband** — detects raw carrier presence, not protocol-level analysis. Strong legitimate transmitters may trigger false positives.
- **~800ms scan cycle** — brief burst jammers shorter than the dwell window may be missed.
- **No logging** — results are display-only, not saved to SD card.

---

## Legal Disclaimer

SubGHz Jammer Detector is for **authorized RF security monitoring and research only.** This tool is a **receive-only** detector and does not transmit or jam any signals. Intentional jamming of radio communications is illegal under FCC Part 15, EU Radio Equipment Directive, and equivalent regulations worldwide.
