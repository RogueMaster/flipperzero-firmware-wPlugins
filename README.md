# UHF Expansion

UHF RFID Reader/Writer expansion for Flipper Zero, communicating over UART bridge.

![uhf_expansion](uhf_expansion.png)

## Features

- **Inventory Scan** — Fast UHF tag scanning with real-time display
- **Paged EPC List** — Browse scanned tags page by page, with truncated preview
- **Tag Details** — View full EPC, RSSI, and PC (Protocol Control) bits for each tag
- **Save CSV Records** — Export scanned tags to CSV file on SD card
- **About Page** — Version info and project links

## Screenshots

| Inventory | Tag List |
|:--:|:--:|
| ![Inventory screen](images/01.png) | ![Tag list screen](images/02.png) |
| **Tag Details** | **Menu** |
| ![Tag details screen](images/03.png) | ![Menu screen](images/04.png) |

## Hardware Setup

### Wiring (UART bridge mode)

| Flipper Zero | UHF Module |
|-------------|------------|
| `13` (TX)   | RX         |
| `14` (RX)   | TX         |
| `16` (C0)   | RST        |
| `8` (3.3V)  | VCC        |
| `18` (GND)  | GND        |

> **Note:** The application is configured to use hardware UART (USART) on pins 13/14 at 115200 baud by default. Software UART fallback on A4/A6 is also supported.

> **Hardware reset:** New board revisions should connect the UCM601NC active-low `RST` pin to Flipper `C0` (external pin 16). The application pulses C0 low before its first version probe, preventing the reader from remaining unresponsive after a failed power-on reset. C0 is reserved for reset and is not used as an LPUART fallback.

### Supported Modules

- UCM601 — UHF RFID transceiver module

## Installation

### Using ufbt

```bash
# Clone the repository
git clone https://github.com/mtoolstec/fz-uhf-expansion.git
cd fz-uhf-expansion

# Build and install
ufbt build
ufbt launch
```

### Manual installation

1. Download the latest `uhf_expansion.fap` from the [Releases](https://github.com/mtoolstec/fz-uhf-expansion/releases) page
2. Copy it to your Flipper Zero's SD card: `SD Card/apps/GPIO/uhf_expansion.fap`
3. Launch from **Apps → GPIO → UHF Expansion**

## Building from Source

Requirements:
- [ufbt](https://pypi.org/project/ufbt/) — Flipper Zero build tool (`pip install ufbt`)

```bash
ufbt build
```

The compiled `.fap` will be at `build/uhf_expansion.fap`.

## Usage

1. Connect your UHF module as described in [Hardware Setup](#hardware-setup)
2. Open **Apps → GPIO → UHF Expansion**
3. Press **OK** to start/stop inventory scanning
4. Navigate the tag list with **Up/Down**
5. Press **OK** on a tag to view details
6. Use the submenu to save or clear tag records

## Protocol

The module communicates using a binary protocol over UART (115200 baud, 8N1):

- **Frame start:** `0xA0`
- **Get version:** `0x72`
- **Start inventory:** `0x89` / `0x8A`
- **Stop inventory:** `0x8C`

Tag responses include EPC (Electronic Product Code), RSSI, and PC bits.

## License

[MIT License](LICENSE)

## Author

[MTools Tec](https://github.com/mtoolstec)

---

*Built for Flipper Zero — https://flipperzero.one*
