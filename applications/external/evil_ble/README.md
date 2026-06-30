# Evil BLE

```
 _____       _ _   ____  _     _____
| ____|_   _(_) | | __ )| |   | ____|
|  _| \ \ / / | | |  _ \| |   |  _|
| |___ \ V /| | | | |_) | |___| |___
|_____| \_/ |_|_| |____/|_____|_____|
```

**BLE device cloning — scan and impersonate Bluetooth Low Energy devices with Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.x-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-Bluetooth-blue)](.)

> **For authorized security testing only. Unauthorized BLE impersonation may violate local laws.**

---

## What It Does

Evil BLE scans for nearby Bluetooth Low Energy devices via an ESP32 running Marauder firmware, then clones a selected device's BLE advertisement using the Flipper Zero's built-in extra_beacon hardware. The Flipper broadcasts as the target device, spoofing its MAC address and device name.

Use cases:
- BLE security assessments — test if systems properly validate device identity beyond name/MAC
- Proximity relay attacks — clone a BLE beacon to test access control systems
- Smart lock testing — verify locks don't rely solely on BLE advertisement matching
- Red team engagements — impersonate trusted BLE peripherals

How it works:
1. ESP32 scans for BLE devices and streams results to Flipper via UART
2. User selects a target device from the list
3. Flipper configures its extra_beacon to broadcast with the target's MAC and advertisement payload
4. Nearby devices see the Flipper as the cloned device

---

## Hardware Requirements

| Component | Details |
|---|---|
| **Flipper Zero** | Running official firmware 1.4.x with extra_beacon API support |
| **ESP32 Dev Board** | With [Marauder firmware](https://github.com/justcallmekoko/ESP32Marauder) (BLE-capable: ESP32-WROOM or ESP32-S3) |
| **Connection** | ESP32 connected to Flipper expansion port (USART, 115200 baud) |

> **Note:** The expansion module is automatically disabled during operation.

---

## Installation

**Build:**
```bash
cd evil_ble
ufbt
```

**Deploy:**
```
dist/evil_ble.fap  ->  /ext/apps/Bluetooth/evil_ble.fap
```

---

## Usage

### Menu Structure

```
Main Menu
├── Scan for Devices       -> Starts BLE scan via ESP32
├── Clone Selected (N)     -> Shows device list for selection
└── Stop Clone [ACTIVE]    -> Stops active broadcast
```

### Workflow

1. Select **Scan for Devices** to start scanning (max 32 devices)
2. Press **Back** to stop scanning
3. Select **Clone Selected** to see the device list
4. Pick a target device — cloning starts immediately
5. The Clone Status screen shows the active broadcast
6. Press **Back** or select **Stop Clone** to stop broadcasting

### Clone Status Screen

```
Broadcasting as:
MySmartLock
MAC: AA:BB:CC:DD:EE:FF
RSSI was: -65 dBm

Press BACK to stop.
```

### Controls

| View | Button | Action |
|---|---|---|
| Main Menu | OK | Enter selected option |
| Device List | OK | Select device and start cloning |
| Clone Status | Back | Stop clone, return to menu |
| Any View | Back | Return to previous view |

---

## Clone Configuration

| Parameter | Value |
|---|---|
| Beacon interval | 100ms |
| Advertising channels | All (37, 38, 39) |
| TX power | 0 dBm |
| Address type | Random |
| Payload | Cloned from scan or synthesized from device name (AD type 0x09) |

The advertisement payload is reconstructed from the Marauder scan data. If only a device name is available, a minimal BLE advertisement structure is synthesized using AD type 0x09 (Complete Local Name), up to 31 bytes.

---

## Architecture

```
ESP32 Marauder --UART 115200--> evil_ble_uart.c (ISR -> RX worker)
                                      |
                                      v
                              evil_ble_scanner.c (parse, dedup, MAC extraction)
                                      |
                                      v (mutex-protected device list)
                                      |
                               evil_ble.c (GUI + extra_beacon clone engine)
```

| File | Purpose |
|---|---|
| `evil_ble.c` / `.h` | Main app, 3-view GUI, clone start/stop via extra_beacon API |
| `evil_ble_scanner.c` / `.h` | Marauder output parsing, device dedup, MAC/name extraction |
| `evil_ble_uart.c` / `.h` | UART abstraction (ISR, stream buffer, line assembly) |

---

## Limitations

- **Max 32 devices** per scan session
- **Cloned advertisement is minimal** — only name-based AD structure, no service UUIDs or manufacturer data
- **Single clone at a time** — can only impersonate one device
- **Requires ESP32 for scanning** — the Flipper's built-in BLE is used for broadcasting
- **0 dBm TX power** — effective range is limited to a few meters

---

## Legal Disclaimer

Evil BLE is for **authorized penetration testing and security research only.** BLE impersonation without authorization may violate computer fraud and wireless communication laws. This tool demonstrates why BLE device identity should not be trusted based solely on MAC address or device name.
