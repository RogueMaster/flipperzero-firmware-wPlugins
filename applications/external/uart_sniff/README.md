# UART Sniff

```
 _   _   _    ____ _____   ____        _  __  __
| | | | / \  |  _ \_   _| / ___| _ __ (_)/ _|/ _|
| | | |/ _ \ | |_) || |   \___ \| '_ \| | |_| |_
| |_| / ___ \|  _ < | |    ___) | | | | |  _|  _|
 \___/_/   \_\_| \_\|_|   |____/|_| |_|_|_| |_|
```

**Real-time UART serial data capture with hex/ASCII display for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.x-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-GPIO-purple)](.)

> **For authorized hardware security research and debugging only.**

---

## What It Does

UART Sniff captures serial UART data in real-time and displays it on the Flipper Zero's screen in hex, ASCII, or combined format. Connect the Flipper's GPIO pins to a target device's UART TX line to passively monitor serial traffic.

Features:
- Real-time hex dump with 100ms refresh rate
- Three display modes: Hex only, ASCII only, or side-by-side (hex + ASCII)
- 6 configurable baud rates (9600 to 230400)
- Two serial channels: USART (GPIO 13/14) or LPUART
- 4KB ring buffer for continuous capture
- Scrollable display showing last 256 bytes (32 lines x 8 bytes)

Use cases:
- Hardware security assessments — sniff UART debug ports on embedded devices
- IoT reverse engineering — capture boot messages, debug output, serial protocols
- Firmware debugging — monitor serial console output
- Protocol analysis — inspect raw UART traffic between devices

---

## Hardware Setup

### Wiring

| Flipper Pin | Target Device | Notes |
|---|---|---|
| **GPIO 13 (RX)** | Target TX | USART channel (default) |
| **GPIO 14 (TX)** | Target RX | Optional, for bidirectional |
| **GND** | Target GND | Required |

For LPUART channel, use the corresponding LPUART pins instead.

> **Important:** The Flipper Zero operates at 3.3V logic levels. Do not connect directly to 5V UART without a level shifter.

---

## Installation

**Build:**
```bash
cd uart_sniff
ufbt
```

**Deploy:**
```
dist/uart_sniff.fap  ->  /ext/apps/GPIO/uart_sniff.fap
```

---

## Usage

### Menu Structure

```
Main Menu
├── Start Sniff  -> Begin capture and show live data
├── Settings     -> Configure baud rate, channel, display mode
└── Clear        -> Clear capture buffer
```

### Controls

| View | Button | Action |
|---|---|---|
| Main Menu | OK | Enter selected option |
| Sniff View | Back | Stop capture, return to menu |
| Settings | Back | Return to menu |

### Sniff View (Both mode)

```
0000: 48 65 6C 6C 6F 20 57 6F  Hello Wo
0008: 72 6C 64 0D 0A 00 00 00  rld.....
0010: 01 02 03 04 05 06 07 08  ........
```

- Hex mode shows raw bytes with addresses
- ASCII mode shows printable characters (non-printable shown as `.`)
- Both mode combines hex and ASCII side by side

### Settings

| Setting | Options | Default |
|---|---|---|
| Baud Rate | 9600 / 19200 / 38400 / 57600 / 115200 / 230400 | 115200 |
| Channel | USART (GPIO 13/14) / LPUART | USART |
| Show | Hex / ASCII / Both | Both |

Settings take effect on the next **Start Sniff**. Change settings before starting capture.

---

## Architecture

```
Target UART TX --GPIO--> ISR (1 byte at a time)
                              |
                              v
                         FuriStreamBuffer (512 bytes)
                              |
                              v
                         Worker thread (batches of 64 bytes)
                              |
                              v (mutex-protected ring buffer, 4KB)
                              |
                         Main thread (100ms timer -> format -> TextBox)
```

| File | Purpose |
|---|---|
| `uart_sniff.c` / `.h` | Main app, 3-view GUI, hex/ASCII formatter, refresh timer |
| `uart_sniff_worker.c` / `.h` | ISR handler, stream buffer, ring buffer, serial port management |

---

## Memory Usage

| Buffer | Size | Purpose |
|---|---|---|
| Ring buffer | 4,096 bytes | Circular capture storage |
| Stream buffer | 512 bytes | ISR to worker thread bridge |
| Display buffer | 1,537 bytes | Formatted text for TextBox |
| Worker stack | 1,024 bytes | Worker thread |
| **Total** | **~7 KB** | Peak heap usage |

---

## Limitations

- **Passive RX only** — captures data from one direction at a time
- **Display window** — shows last 256 bytes; older data is overwritten in the ring buffer
- **No SD card logging** — data is display-only (not saved to file)
- **No flow control** — RTS/CTS not supported
- **3.3V logic only** — requires level shifter for 5V UART targets
- **Expansion module disabled** during capture (shares USART pins)

---

## Legal Disclaimer

UART Sniff is for **authorized hardware security research and debugging only.** Intercepting serial communications on devices you do not own or have authorization to test may violate local laws.
