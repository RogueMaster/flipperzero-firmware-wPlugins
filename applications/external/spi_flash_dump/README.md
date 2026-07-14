# SPI Flash Dump

```
 ____  ____ ___   _____ _           _
/ ___||  _ \_ _| |  ___| | __ _ ___| |__
\___ \| |_) | |  | |_  | |/ _` / __| '_ \
 ___) |  __/| |  |  _| | | (_| \__ \ | | |
|____/|_|  |___| |_|   |_|\__,_|___/_| |_|
 ____
|  _ \ _   _ _ __ ___  _ __
| | | | | | | '_ ` _ \| '_ \
| |_| | |_| | | | | | | |_) |
|____/ \__,_|_| |_| |_| .__/
                       |_|
```

**SPI NOR flash reader via Flipper Zero GPIO with hex viewer**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-GPIO%2FHardware-green)](.)

> **For authorized hardware security research, firmware extraction, and educational use only.**

---

## What It Does

SPI Flash Dump reads the contents of SPI NOR flash chips directly via the Flipper Zero's GPIO pins, using bit-banged SPI. It supports common flash ICs found in routers, IoT devices, embedded systems, and debug boards.

Features:
- Full flash dump to SD card (binary `.bin` file)
- Live hex viewer with ASCII sidebar (navigable on Flipper screen)
- Auto-detection of flash chip ID (JEDEC vendor + device ID)
- Configurable SPI clock speed
- Supports 24-bit and 32-bit address modes (flashes up to 128 MB)

Use cases:
- Firmware extraction for reverse engineering and vulnerability research
- Bootloader extraction from IoT devices
- Secret/key extraction from embedded systems
- Hardware security assessment

---

## Installation

**Build:**
```bash
cd spi_flash_dump
ufbt
```

**Deploy:**
```
dist/spi_flash_dump.fap  →  /ext/apps/GPIO/spi_flash_dump.fap
```

---

## Wiring

Connect the target flash chip to the Flipper Zero GPIO header:

| Flipper GPIO Pin | Signal | Flash Pin | Notes |
|---|---|---|---|
| Pin 2 (A7) | CS# | CE# / CS# | Chip select (active low) |
| Pin 3 (A6) | CLK | CLK / SCK | SPI clock |
| Pin 4 (B3) | MOSI | DI / SI | Master out |
| Pin 5 (B2) | MISO | DO / SO | Master in |
| Pin 9 (3.3V) | VCC | VCC | 3.3V power |
| Pin 8 or 11 (GND) | GND | GND | Ground |

> **Voltage:** Flipper GPIO operates at 3.3V. Do **not** connect 5V flash chips directly. Use a level shifter.

> **In-circuit reading:** If reading flash while it remains on its board, ensure the host system is powered off first. Other devices sharing the SPI bus may interfere.

**Common flash chip pinouts:**

| Package | CS# | CLK | DO | DI | VCC | GND |
|---|---|---|---|---|---|---|
| SOIC-8 / DIP-8 | Pin 1 | Pin 6 | Pin 2 | Pin 5 | Pin 8 | Pin 4 |
| WSON-8 | Pin 1 | Pin 6 | Pin 2 | Pin 5 | Pin 8 | Pin 4 |

---

## Usage

### Reading a Flash Chip

1. Wire the flash chip as shown above
2. Open `SPI Flash Dump` from the GPIO apps menu
3. The app auto-detects the chip via JEDEC ID command (`9F h`)
4. If detected: chip name, size, and page size are shown
5. Press **OK** to begin dump
6. Progress bar shows completion percentage
7. Dump saved to: `/ext/spi_dumps/<chipname>_<timestamp>.bin`

### Hex Viewer

After a successful dump (or to view a previous dump):
1. Select **View Dump** from the main menu
2. Navigate with **Up/Down** (one line) or **Left/Right** (one page)
3. Hold **OK** to jump to a specific offset (hex input)

**Hex viewer layout:**
```
Offset    Hex                    ASCII
00000000  4D 5A 90 00 03 00 00  MZ......
00000008  00 04 00 00 FF FF 00  ........
00000010  B8 00 00 00 00 00 00  ........
```

---

## Supported Flash Chips

Auto-detected via JEDEC ID:

| Manufacturer | Series | Size Range |
|---|---|---|
| Winbond | W25Q series | 512KB – 128MB |
| Macronix | MX25L series | 512KB – 64MB |
| GigaDevice | GD25Q series | 512KB – 128MB |
| Micron | N25Q / MT25Q | 8MB – 512MB |
| Spansion/Cypress | S25FL series | 4MB – 64MB |
| ISSI | IS25LP series | 512KB – 128MB |

Unrecognized chips: JEDEC ID is displayed and a raw dump is still attempted using configurable size.

---

## Commands Sent to Flash

SPI Flash Dump uses standard SPI NOR commands:

| Command | Hex | Description |
|---|---|---|
| RDID | 9F | Read JEDEC ID (manufacturer + device) |
| RDSR | 05 | Read Status Register (check WIP bit) |
| READ | 03 | Read Data (24-bit address) |
| FAST_READ | 0B | Fast Read (with dummy byte) |
| READ4 | 13 | Read Data (32-bit address, >16MB chips) |

Write and erase commands are **not** implemented — this is a read-only tool.

---

## Output Files

Dumps are saved to the Flipper SD card:

```
/ext/spi_dumps/
├── W25Q128_20260302_091400.bin   (raw binary, 16,777,216 bytes)
├── MX25L6406_20260301_154320.bin
└── UNKNOWN_9F020000_20260228.bin
```

**Analyzing the dump on your PC:**

```bash
# Identify filesystem or firmware format
binwalk firmware.bin

# Extract embedded filesystems
binwalk -e firmware.bin

# Search for strings
strings firmware.bin | grep -i password

# Entropy analysis
binwalk -E firmware.bin

# Open in hex editor
hexdump -C firmware.bin | less
# or: xxd firmware.bin | less
```

---

## SD Card Layout

```
/ext/
├── apps/
│   └── GPIO/
│       └── spi_flash_dump.fap
└── spi_flash_dump/
    ├── W25Q128_20260302.bin
    └── ...
```

---

## Legal Disclaimer

SPI Flash Dump is for **authorized hardware security research and firmware analysis only.**
Dumping firmware from devices you do not own or have explicit written permission to analyze may violate the CFAA, DMCA, and equivalent laws worldwide.
