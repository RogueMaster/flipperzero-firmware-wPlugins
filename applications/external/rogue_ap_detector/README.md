# Rogue AP Detector

```
 ____                          _    ____
|  _ \ ___   __ _ _   _  ___  / \  |  _ \
| |_) / _ \ / _` | | | |/ _ \/ _ \ | |_) |
|  _ < (_) | (_| | |_| |  __/ ___ \|  __/
|_| \_\___/ \__, |\__,_|\___/_/   \_\_|
            |___/
 ____       _            _
|  _ \  ___| |_ ___  ___| |_ ___  _ __
| | | |/ _ \ __/ _ \/ __| __/ _ \| '__|
| |_| |  __/ ||  __/ (__| || (_) | |
|____/ \___|\__\___|\___|\__\___/|_|
```

**Evil twin and rogue WiFi access point detection for Flipper Zero via ESP32 Marauder**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.x-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-WiFi-green)](.)

> **For authorized WiFi security assessments and network defense only.**

---

## What It Does

Rogue AP Detector monitors the WiFi environment for evil twin attacks and rogue access points. It uses an external ESP32 running Marauder firmware to scan for WiFi networks, then analyzes the results to detect:

- **Duplicate SSIDs** from different MAC addresses (BSSID) — indicates a possible rogue AP
- **RSSI anomalies** — an attacker AP broadcasting the same SSID at significantly higher power (>20 dBm delta) strongly indicates an evil twin attack

Three threat levels:
- **CLEAN** — no suspicious networks detected
- **SUSPECT** — same SSID seen from 2+ BSSIDs (could be legitimate multi-AP deployment)
- **EVIL TWIN** — SUSPECT + significant RSSI delta between BSSIDs (likely attack)

Use cases:
- Red team WiFi engagements — verify your evil twin is visible and behaving as expected
- Blue team defense — detect rogue APs on corporate networks
- Wireless security audits — identify unauthorized access points
- Education — demonstrate evil twin attacks in a lab setting

---

## Hardware Requirements

| Component | Details |
|---|---|
| **Flipper Zero** | Running official firmware 1.4.x (API 87.1) |
| **ESP32 Dev Board** | With [Marauder firmware](https://github.com/justcallmekoko/ESP32Marauder) |
| **Connection** | ESP32 connected to Flipper expansion port (USART, 115200 baud) |

The ESP32 handles all WiFi scanning. The Flipper runs the detection logic and UI.

> **Note:** The expansion module is automatically disabled while this app runs, since it shares the same USART pins.

---

## Installation

**Build:**
```bash
cd rogue_ap_detector
ufbt
```

**Deploy:**
```
dist/rogue_ap_detector.fap  →  /ext/apps/WiFi/rogue_ap_detector.fap
```

---

## Usage

### Menu Structure

```
Main Menu
├── Scan        → Live scanning view with threat status
├── Results     → Detailed list of flagged SSIDs and BSSIDs
└── Settings    → Min RSSI filter
```

### Controls

| View | Button | Action |
|---|---|---|
| Main Menu | OK | Enter selected option |
| Main Menu | Back | Exit app |
| Scan | OK | Start / Stop scanning |
| Scan | Back | Return to menu |
| Results | Up/Down | Scroll through results |
| Results | Back | Return to menu |

### Scan View

```
Rogue AP Detector           [live]
──────────────────────────────────
Status: ██ EVIL TWIN !!

APs seen: 24

MyNetwork (3 BSSIDs)
```

The status indicator shows the current threat level:
- Empty box = **CLEAN**
- Dotted box = **SUSPECT**
- Inverted bar = **EVIL TWIN** (with red LED blink + vibration alert)

### Results View

When threats are detected, the Results view shows:

```
[EVIL_TWIN] CorporateWiFi
  AA:BB:CC:DD:EE:FF  -45 dBm  Ch 6
  11:22:33:44:55:66  -72 dBm  Ch 6
  77:88:99:AA:BB:CC  -68 dBm  Ch 1

[SUSPECT] GuestNetwork
  DD:EE:FF:00:11:22  -55 dBm  Ch 11
  33:44:55:66:77:88  -58 dBm  Ch 11

Total APs: 24
```

### Alerts

| Threat Level | Notification |
|---|---|
| SUSPECT | Yellow LED blink |
| EVIL TWIN | Red LED blink + vibration |

Alerts fire once per threat escalation and reset when the threat clears.

---

## Settings

| Setting | Options | Default |
|---|---|---|
| Min RSSI | -50, -60, -70, -80, -90 dBm | -90 dBm |

> **Note:** The min RSSI filter is stored in-session only (not persisted to SD card). APs with RSSI weaker than the threshold are dropped before detection analysis.

---

## Detection Algorithm

1. ESP32 Marauder runs `scanap` and streams AP data (SSID, BSSID, RSSI, channel)
2. Flipper parses each line and upserts into an AP table (up to 128 entries)
3. Entries not seen for 30 seconds are pruned as stale
4. After each update, the detection engine scans for SSIDs with 2+ distinct BSSIDs:
   - If found → **SUSPECT**
   - If the RSSI delta between any two BSSIDs exceeds 20 dBm → **EVIL TWIN**
5. UI refreshes every 500ms with a snapshot of the current state

---

## Architecture

```
ESP32 Marauder ──UART 115200──► rogue_uart.c (ISR → stream buffer → RX worker)
                                      │
                                      ▼
                              rogue_ap_worker.c (parse, upsert, detect)
                                      │
                                      ▼ (mutex-protected RogueApResults)
                                      │
                               rogue_ap.c (500ms timer → view model → canvas)
```

| File | Purpose |
|---|---|
| `rogue_ap.c` / `.h` | Main app, GUI (4 views), entry point, refresh timer |
| `rogue_ap_worker.c` / `.h` | AP parsing, upsert table, evil twin detection |
| `rogue_uart.c` / `.h` | UART abstraction (ISR, stream buffer, line assembly) |

---

## Limitations

- **Requires ESP32 + Marauder** — the Flipper Zero has no WiFi hardware
- **2.4 GHz only** — 5 GHz networks are not scanned (ESP32 Marauder limitation)
- **Max 128 APs** — in dense environments, the oldest entries are not evicted (table fills up)
- **Legitimate multi-AP networks** (enterprise, mesh) will trigger SUSPECT — use RSSI delta and channel info to distinguish from actual attacks
- **SSIDs ending in digits** (e.g., "WiFi 5") may be truncated by the Marauder output parser's beacon-byte stripping heuristic

---

## Legal Disclaimer

Rogue AP Detector is for **authorized WiFi security assessments and network defense only.** Deploying evil twin access points without authorization is illegal under the Computer Fraud and Abuse Act (US), Computer Misuse Act (UK), and equivalent laws worldwide. This tool is a **passive detector** — it does not create or interfere with WiFi networks.
