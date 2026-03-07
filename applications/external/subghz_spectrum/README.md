# SubGHz Spectrum

```
 ____        _      ____  _
/ ___| _   _| |__  / ___|| |__  ____
\___ \| | | | '_ \| |  _ | '_ \|_  /
 ___) | |_| | |_) | |_| || | | |/ /
|____/ \__,_|_.__/ \____||_| |_/___|
 ____                  _
/ ___| _ __   ___  ___| |_ _ __ _   _ _ __ ___
\___ \| '_ \ / _ \/ __| __| '__| | | | '_ ` _ \
 ___) | |_) |  __/ (__| |_| |  | |_| | | | | | |
|____/| .__/ \___|\___|\__|_|   \__,_|_| |_| |_|
      |_|
```

**SubGHz spectrum analyzer with bar chart and waterfall views for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-SubGHz-red)](.)

> **For authorized RF security research, spectrum monitoring, and educational use only.**

---

## What It Does

SubGHz Spectrum turns Flipper Zero into a real-time sub-GHz spectrum analyzer using the CC1101 radio transceiver. It sweeps across a configurable frequency range and displays the RSSI (signal strength) at each frequency in two views:

- **Bar view** — vertical bar chart showing current RSSI at each frequency step
- **Waterfall view** — scrolling time-frequency display showing signal history over time

Use cases:
- RF security assessments (identify active frequencies and signal patterns)
- Locating unknown transmitters and jammers
- Characterizing the RF environment before conducting SubGHz attacks
- Identifying garage door openers, key fobs, and other devices
- Verifying that RF-based security systems are actually transmitting

---

## Installation

**Build:**
```bash
cd subghz_spectrum
ufbt
```

**Deploy:**
```
dist/subghz_spectrum.fap  →  /ext/apps/Sub-GHz/subghz_spectrum.fap
```

---

## Supported Frequency Ranges

The Flipper Zero's CC1101 supports:

| Band | Range |
|---|---|
| 300–348 MHz | Low sub-GHz (some garage openers, alarms) |
| 387–464 MHz | Main band (315 MHz, 433 MHz key fobs, sensors) |
| 779–928 MHz | High band (868 MHz EU, 915 MHz US ISM, 900 MHz IoT) |

Common center frequencies:
- **315 MHz** — North American garage doors, key fobs
- **433.92 MHz** — European/global key fobs, sensors, RC
- **868 MHz** — European ISM (Z-Wave EU, LoRa EU)
- **915 MHz** — US ISM (Z-Wave US, LoRa US, LPWAN)

---

## Usage

### Controls

| Button | Action |
|---|---|
| **OK** | Toggle between Bar view and Waterfall view |
| **Up** | Increase center frequency |
| **Down** | Decrease center frequency |
| **Left** | Narrow frequency span (zoom in) |
| **Right** | Widen frequency span (zoom out) |
| **Hold Up/Down** | Jump frequency by 10× step |
| **Back** | Exit |

### Bar View

```
  SubGHz Spectrum — 433.92 MHz ± 2 MHz
  ┌────────────────────────────────────┐
  │            ██                      │ -40 dBm
  │         ████ ██                    │
  │      ████████████                  │ -60 dBm
  │   ████████████████ ██              │
  │█████████████████████████████████   │ -80 dBm
  └────────────────────────────────────┘
   430                433.92          436
```

The horizontal axis shows frequency; the vertical axis shows RSSI. The peak is highlighted.

### Waterfall View

```
  SubGHz Spectrum — 433.92 MHz ± 2 MHz
  ┌────────────────────────────────────┐  ← newest
  │░░░░░░░░░████████████████░░░░░░░░░  │
  │░░░░░░░░█████████████████░░░░░░░░░  │
  │░░░░░░░░░████████████████░░░░░░░░░  │
  │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
  │░░░░░░██░░░░░░░░░░░░░░░░░░░░░░░░░░  │  ← oldest
  └────────────────────────────────────┘
   430                433.92          436
```

Each horizontal line is one sweep. Dark = strong signal; light = noise floor. Persistent signals appear as vertical bands; bursty signals appear as isolated horizontal marks.

---

## Settings

Accessible from the main menu before starting:

| Setting | Options | Default |
|---|---|---|
| Center frequency | Any supported CC1101 frequency | 433.92 MHz |
| Span | 500 kHz — 20 MHz | 4 MHz |
| Step size | 10 kHz — 500 kHz | Span ÷ 64 |
| Dwell time | 1 ms — 50 ms per step | 5 ms |
| Gain | Low / Mid / High / Max | Mid |
| View | Bar / Waterfall | Bar |

---

## Interpreting Results

**What you're seeing:**

| Pattern | Likely Cause |
|---|---|
| Narrow spike, brief | Key fob transmission, button press |
| Wide flat bump | Broadband device (FSK/OOK with deviation) |
| Persistent strong signal | Nearby transmitter (alarm, sensor, baby monitor) |
| Noise floor jump | Interference or jamming |
| Regular repeated burst | Sensor reporting on schedule |

**RSSI reference levels (typical):**

| RSSI | Interpretation |
|---|---|
| > -50 dBm | Very strong — device is nearby |
| -50 to -70 dBm | Strong — within a few meters |
| -70 to -85 dBm | Moderate — same building |
| -85 to -100 dBm | Weak — edge of range or through walls |
| < -100 dBm | Near noise floor |

---

## Combining With Other Tools

SubGHz Spectrum is useful before using other Flipper Zero SubGHz features:

1. **Use SubGHz Spectrum** to identify what frequencies are active and what devices are transmitting
2. **Use Flipper's built-in SubGHz Read** on the identified frequency to capture and decode the protocol
3. **Use Flipper's SubGHz Send** to replay captured signals

For security assessments:
1. Spectrum → identify RF-based security sensors and their frequencies
2. Capture → record their transmissions
3. Analysis → determine replay vulnerability (rolling code vs. fixed code)

---

## SD Card Layout

```
/ext/
└── apps/
    └── Sub-GHz/
        └── subghz_spectrum.fap
```

No data files are saved to SD by the spectrum analyzer — it is a real-time display tool only.

---

## Legal Disclaimer

SubGHz Spectrum is for **authorized RF security research and spectrum monitoring only.**
Transmitting on frequencies without the appropriate license, or jamming communications, is illegal under FCC Part 97, Part 15, and equivalent regulations worldwide.
This app is a **receive-only** spectrum analyzer and does not transmit.
