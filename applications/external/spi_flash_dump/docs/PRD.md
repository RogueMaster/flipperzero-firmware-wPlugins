# PRD: SPI Flash Dumper (`spi_flash_dump`)

## Overview
A tool for reading SPI NOR flash memory chips via the Flipper Zero's GPIO header.
Identifies chips by JEDEC ID, reads full contents to SD card, and verifies
integrity. Essential for firmware extraction during hardware security assessments.

## Problem Statement
Firmware extraction from SPI flash is one of the most common hardware pentest
tasks. Currently requires a dedicated programmer ($20-50 CH341A, $200+ Xeltek)
plus a laptop. The Flipper Zero has SPI-capable GPIO pins and an SD card — it
can do this standalone, letting hardware pentesters dump firmware in the field
without carrying extra equipment.

## Target Users
- Hardware security pentesters extracting device firmware
- IoT security researchers analyzing embedded systems
- Reverse engineers needing flash dumps for analysis

## Hardware Utilized
- **GPIO SPI** via `furi_hal_spi` (or bit-banged SPI via `furi_hal_gpio`)
- **SD card** via `storage` API for saving dumps
- **Display** for progress and chip info

## Supported Hardware

### SPI Flash Chip Families
| Manufacturer | Series          | Capacities        |
|-------------|-----------------|-------------------|
| Winbond     | W25Q            | 512Kbit - 128Mbit |
| Macronix    | MX25L           | 512Kbit - 128Mbit |
| Microchip   | SST25/SST26     | 512Kbit - 32Mbit  |
| ISSI        | IS25LP/IS25WP   | 512Kbit - 128Mbit |
| GigaDevice  | GD25Q           | 512Kbit - 128Mbit |
| Spansion    | S25FL           | 512Kbit - 128Mbit |
| Generic     | Any JEDEC-compliant SPI NOR          |

### Wiring
```
Flipper GPIO → SPI Flash
─────────────────────────
Pin 2 (A7)  → CLK  (SPI Clock)
Pin 3 (A6)  → MISO (Data Out / DO)
Pin 4 (A4)  → MOSI (Data In / DI)
Pin 5 (B3)  → CS   (Chip Select)
Pin 8 (GND) → GND
Pin 9 (3V3) → VCC  (3.3V power)
```

## Core Features

### F1 — Chip Detection
- Send JEDEC Read ID command (0x9F)
- Look up manufacturer + device ID in built-in database
- Display: Manufacturer, part number, capacity, voltage range
- If unknown JEDEC ID: display raw bytes, allow manual size entry

### F2 — Full Chip Read
- Read entire flash contents using READ (0x03) or FAST READ (0x0B) command
- Page-by-page (256 bytes) with progress bar
- Save to SD card: `/ext/spi_dumps/CHIPNAME_YYYYMMDD_HHMMSS.bin`
- Estimated time displayed before starting
- Pause/Resume support

### F3 — Verify (Read-Back Compare)
- After initial read, optionally re-read and compare byte-by-byte
- Reports: match, mismatch count, first mismatch address
- CRC32 of dump displayed for external verification

### F4 — Partial Read
- Read specific address range: start address + length
- Useful for extracting specific partitions or regions
- Hex input for addresses

### F5 — Chip Info View
Display detailed chip information:
```
┌──────────────────────────────┐
│ SPI Flash Chip Info          │
│                              │
│ Mfr:  Winbond (0xEF)        │
│ Part: W25Q128JV              │
│ Size: 16,777,216 bytes (16M) │
│ Pages: 65,536 × 256 bytes   │
│ JEDEC: EF 40 18             │
│ Status: 0x00 (unlocked)     │
│                              │
│ [OK: Read]    [>: Options]   │
└──────────────────────────────┘
```

### F6 — Status Register
- Read status register(s): SR1, SR2, SR3
- Display protection bits, write enable, busy status
- Decode block protection regions

### F7 — Hex Preview
- On-device hex viewer of dumped data (first 4KB)
- Scrollable, shows ASCII alongside hex
- Useful for quick identification (looking for firmware headers, strings)

### F8 — Settings
- SPI clock speed: 1 MHz / 2 MHz / 4 MHz / 8 MHz
- Read command: Standard (0x03) / Fast (0x0B)
- Verify after read: On / Off
- Output filename format: Auto / Custom

## SPI Flash Command Set Used
```c
#define CMD_READ_JEDEC_ID    0x9F  // Read manufacturer + device ID
#define CMD_READ_DATA        0x03  // Read data (up to ~33MHz)
#define CMD_FAST_READ        0x0B  // Fast read (with dummy byte)
#define CMD_READ_STATUS_REG1 0x05  // Read status register 1
#define CMD_READ_STATUS_REG2 0x35  // Read status register 2
#define CMD_READ_STATUS_REG3 0x15  // Read status register 3
#define CMD_WRITE_ENABLE     0x06  // Write enable (needed for status read on some chips)
#define CMD_RELEASE_PWRDOWN  0xAB  // Release from power-down + read device ID
```

Note: This tool is READ-ONLY. No erase or write commands are implemented to
prevent accidental data destruction.

## Architecture
```
main ──► SceneManager + ViewDispatcher
           ├── detect_scene (Widget — shows chip info or "not found")
           ├── read_scene (Custom View — progress bar + stats)
           ├── verify_scene (Custom View — comparison progress)
           ├── hex_viewer (Custom View — scrollable hex dump)
           ├── partial_read (ByteInput for start addr + NumberInput for length)
           └── settings (VariableItemList)

spi_worker (FuriThread)
  ├── chip_detect(): send JEDEC ID, look up in database
  ├── chip_read(): page-by-page read, write to SD stream
  ├── chip_verify(): re-read and compare against saved file
  └── reports progress via FuriMessageQueue to UI
```

## Key API Calls
```c
// SPI communication (using hardware SPI or bit-banged GPIO)
// Option A: Hardware SPI
furi_hal_spi_bus_handle_init(&spi_handle);
furi_hal_spi_acquire(&spi_handle);
furi_hal_spi_bus_tx(&spi_handle, tx_data, tx_len, timeout);
furi_hal_spi_bus_rx(&spi_handle, rx_data, rx_len, timeout);
furi_hal_spi_release(&spi_handle);

// Option B: Bit-banged GPIO (more flexible pin assignment)
furi_hal_gpio_write(&gpio_cs, false);   // CS low
// Clock out bytes manually via gpio_clk, gpio_mosi, gpio_miso
furi_hal_gpio_write(&gpio_cs, true);    // CS high

// SD card storage
Storage* storage = furi_record_open(RECORD_STORAGE);
File* file = storage_file_alloc(storage);
storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
storage_file_write(file, buffer, size);
```

## JEDEC ID Database (Built-in)
Compiled-in lookup table with ~50 most common SPI flash parts:
```c
typedef struct {
    uint8_t manufacturer_id;
    uint8_t device_id[2];
    const char* manufacturer_name;
    const char* part_name;
    uint32_t size_bytes;
} SpiFlashChipInfo;
```

## Safety
- **READ-ONLY**: No write/erase commands implemented. Cannot brick target device.
- **3.3V only**: Warning screen about voltage. 1.8V chips need level shifter.
- **Wiring guide**: Shown on screen before first use with pinout diagram.

## Out of Scope
- Writing / erasing flash (intentionally excluded for safety)
- NAND flash (different protocol and page structure)
- eMMC / SD card reading via SPI (different command set)
- Parallel flash (NOR or NAND)
- 1.8V chip support (would need level shifter hardware)

## Success Criteria
- Correctly identifies W25Q128 chip via JEDEC ID
- Full 16MB dump completes in < 15 minutes at 4 MHz SPI clock
- Dump is bit-identical to reference dump from CH341A programmer
- Verify pass confirms 100% match after successful read
- Hex preview correctly shows firmware header (e.g., U-Boot magic bytes)
- Handles missing/unconnected chip gracefully (no hang, clear error message)
