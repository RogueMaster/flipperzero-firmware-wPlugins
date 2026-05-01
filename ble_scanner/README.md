# BLE Scanner

```
 ____  _     _____   ____
| __ )| |   | ____| / ___|  ___ __ _ _ __  _ __   ___ _ __
|  _ \| |   |  _|   \___ \ / __/ _` | '_ \| '_ \ / _ \ '__|
| |_) | |___| |___   ___) | (_| (_| | | | | | | |  __/ |
|____/|_____|_____| |____/ \___\__,_|_| |_|_| |_|\___|_|
```

**Bluetooth Low Energy device scanner with AirTag detection for Flipper Zero via ESP32 Marauder**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.x-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-Bluetooth-blue)](.)

> **For authorized Bluetooth security assessments and research only.**

---

## What It Does

BLE Scanner discovers nearby Bluetooth Low Energy devices using an external ESP32 running Marauder firmware. It displays each device's signal strength (RSSI), MAC address, and name in a scrollable list with optional SD card logging.

Features:
- Real-time BLE device enumeration with 500ms refresh
- Apple AirTag detection (via name matching and OUI lookup)
- Configurable RSSI filtering and sorting (by signal strength, recency, or MAC)
- SD card logging to TSV files for post-scan analysis
- Tracks up to 64 devices per session with deduplication

Use cases:
- Bluetooth security assessments — enumerate nearby BLE devices
- AirTag/tracker detection — find unwanted tracking devices
- BLE reconnaissance — identify device names and MAC addresses before targeted attacks
- RF environment surveys — catalog BLE devices in an area

---

## Hardware Requirements

| Component | Details |
|---|---|
| **Flipper Zero** | Running official firmware 1.4.x (API 87.1) |
| **ESP32 Dev Board** | With [Marauder firmware](https://github.com/justcallmekoko/ESP32Marauder) (must have BLE support — ESP32-WROOM or ESP32-S3, NOT ESP32-S2) |
| **Connection** | ESP32 connected to Flipper expansion port (USART, 115200 baud) |

> **Note:** The expansion module is automatically disabled while this app runs.

---

## Installation

**Build:**
```bash
cd ble_scanner
ufbt
```

**Deploy:**
```
dist/ble_scanner.fap  ->  /ext/apps/Bluetooth/ble_scanner.fap
```

---

## Usage

### Menu Structure

```
Main Menu
├── Scan      -> Live device list (scrollable)
└── Settings  -> RSSI filter, sort mode, SD logging
```

### Controls

| View | Button | Action |
|---|---|---|
| Main Menu | OK | Enter selected option |
| Main Menu | Back | Exit app |
| Scan | Up/Down | Scroll device list |
| Scan | Back | Stop scan, return to menu |

### Scan View

```
BLE Scanner                [scanning]
----------------------------------
RSSI MAC               Name
 -45 AA:BB:CC:DD:EE:FF MyPho
 -52 11:22:33:44:55:66 !TAG!
 -68 77:88:99:AA:BB:CC
 -71 DD:EE:FF:00:11:22 Watch
 -83 DE:VI:CE:12:34:56 Speak
```

- Devices with no MAC in the Marauder output get a deterministic placeholder MAC (`DE:VI:CE:xx:xx:xx`) derived from a hash of their name
- AirTag detection uses both name matching ("airtag") and known Apple OUI prefixes

---

## Settings

| Setting | Options | Default |
|---|---|---|
| Min RSSI | Any (-100) / -80 / -60 / -40 dBm | Any |
| Sort by | RSSI / Time / MAC | RSSI |
| Log to SD | Off / On | Off |

When SD logging is enabled, scan results are saved to:
```
/ext/ble_scanner/scan_YYYYMMDD_HHMMSS.log
```

Log format (TSV):
```
RSSI	MAC	Name
-65	AA:BB:CC:DD:EE:FF	MyPhone
-70	DE:VI:CE:12:34:56	Unknown Device
```

---

## Architecture

```
ESP32 Marauder --UART 115200--> ble_uart.c (ISR -> stream buffer -> RX worker)
                                     |
                                     v
                             ble_scanner_worker.c (parse, dedup, AirTag detect)
                                     |
                                     v (mutex-protected BleScanResults)
                                     |
                              ble_scanner.c (500ms timer -> filter/sort -> view)
```

| File | Purpose |
|---|---|
| `ble_scanner.c` / `.h` | Main app, GUI, menu, settings, refresh timer |
| `ble_scanner_worker.c` / `.h` | Marauder output parsing, device table, AirTag heuristic, SD logging |
| `ble_uart.c` / `.h` | UART abstraction (ISR, stream buffer, line assembly) |

---

## Limitations

- **Requires ESP32 + Marauder** with BLE support (ESP32-S2 has no BLE radio)
- **Max 64 devices** per scan session
- **Device names truncated** to 5 characters on the 128x64 display
- **AirTag detection is heuristic** — based on name and OUI, not service UUID advertisement parsing

---

## Legal Disclaimer

BLE Scanner is for **authorized Bluetooth security research only.** This tool is a **passive scanner** — it does not transmit BLE advertisements or interfere with Bluetooth communications.
