# Changelog

All notable changes to the 24cxxprog EEPROM Programmer application will be documented in this file.

## [2.1.0] - 2026-04-21

### 🐛 Bug Fixes

#### Critical: Correct I2C Addressing for 24C32 and Larger Chips

- **Fixed write operations silently doing nothing on 24C256 and larger chips**: chips from 24C32 onward require a 2-byte memory address in the I2C transaction (`[addrHigh, addrLow, data...]`). The driver was sending only a single address byte for all chip types, causing the EEPROM to misinterpret the address as data and write nothing to the intended location.
- **Fixed reads always returning data from address 0x00 on large chips**: the same 1-byte addressing bug caused the read seek phase to always land at `(realAddr & 0xFF)` instead of the correct address. Data programmed by other programmers above offset 0xFF was unreadable.
- `EEPROM24C02` class renamed to `EEPROM24Cxx` to reflect support for the full chip family. All address parameters widened from `uint8_t` to `uint16_t` (covers the full 0–65535 range of the 24C512).
- Added `EEPROMAddrWidth` enum (`EEPROM_ADDR_1BYTE` / `EEPROM_ADDR_2BYTE`) stored as a driver field and selected automatically based on chip type.

#### Critical: Correct Page Size per Chip Type

- **Fixed data corruption on page-boundary writes**: the driver hardcoded a page size of 8 bytes for all chip types. Writing data that crossed what the chip considers a page boundary caused the EEPROM's internal write pointer to wrap within the page, silently writing bytes to the wrong address within that page.
- Page size is now a driver field set per chip type at runtime :

  | Chip                  | Page size |
  |-----------------------|-----------|
  | 24C01 / 24C02         | 8 bytes   |
  | 24C04 / 24C08 / 24C16 | 16 bytes  |
  | 24C32 / 24C64         | 32 bytes  |
  | 24C128 / 24C256       | 64 bytes  |
  | 24C512                | 128 bytes |

#### High: Flipper crash/reboot when selecting 24C512

- **Fixed out-of-memory crash**: `reallocate_buffers()` attempted to allocate three 64 KB buffers simultaneously (192 KB total) on a device with 256 KB of RAM shared with the OS. One or more `malloc()` calls would fail, returning a null pointer that was immediately dereferenced.
- Allocation is now wrapped in a retry loop that halves `new_size` until all three buffers succeed. All pointers are null-checked after every `malloc()` and immediately nulled after `free()` to prevent double-free.

#### Medium: Verify always passing on chips larger than 256 bytes

- **Fixed silent verify false-positive**: the post-write verification loop used a `uint8_t` counter, which wraps to 0 after 256 iterations. For any chip larger than 256 bytes the loop exited early and reported success regardless of the actual data. Counter widened to `uint32_t`.

#### Low: Partial erase ignoring start address and length

- **Fixed erase always starting from address 0**: `erase_memory_range()` declared `start_addr` and `length` parameters but marked both as `UNUSED`, always erasing from address 0. Added `erase_end_addr` field to `EEPROMApp`; the function now stores `start_addr + length` as the stop condition used by the async erase step.

#### Low: File browser showing all files instead of recognised extensions only

- **Fixed `is_valid_extension()` dead code**: an early `return true` made the extension-checking logic unreachable, causing every file on the SD card to appear in the file browser. Removed the stray return; only `.bin`, `.hex`, `.dat`, `.raw`, `.eeprom`, and `.rom` files are now listed.

### 🔧 Technical Changes

- Introduced `get_addr_width(EEPROMType)` and `get_page_size(EEPROMType)` helpers; `reallocate_buffers()` now calls `eeprom->setChipParams()` on every chip-type change so the driver always operates with the correct protocol.
- All `readBytes()` / `writeBytes()` call sites updated to use `uint16_t` chunk sizes and explicit casts.
- `process_erase_step()` now uses `eeprom->getPageSize()` for chunk sizing and a 128-byte stack buffer (up from 8 bytes) to match the largest supported page.
- `save_memory_to_file()` now reads the EEPROM in 16-byte chunks rather than a single bulk read, ensuring the address fits `uint16_t` and the I2C transaction stays within reliable transfer sizes.
- CI workflow updated: `actions/upload-artifact` bumped from v3 (deprecated) to v4. Added keyword-gated steps — `[build]` triggers compilation and artifact upload, `[lint]` triggers `clang-format` auto-commit.

### ⚠️ Notes

- The `EEPROM24C02` class and `EEPROM_24C02_*` constants have been removed. Any out-of-tree code referencing them should be updated to `EEPROM24Cxx` and `EEPROM_24CXX_*`.

---

## [2.0.0] - 2026-03-11

### 🚀 Major Features Added

#### Dynamic Memory Support for All 24Cxx Chips

- **Full chip type support**: Added complete support for all EEPROM sizes from 24C01 (128B) to 24C512 (64KB)
- **Dynamic buffer allocation**: Memory buffers now automatically resize based on selected chip type
- **Configurable in Settings**: Users can now select chip type in Settings menu, and all operations adapt automatically

### ✨ Enhancements

#### Memory Management

- Replaced fixed 256-byte buffers with dynamic allocation:
  - `memory_data` - dynamically allocated based on chip size
  - `file_data` - dynamically allocated based on chip size  
  - `verify_buffer` - dynamically allocated based on chip size
- Added `get_eeprom_size()` helper function returning size in bytes for each chip type
- Added `reallocate_buffers()` function for automatic buffer reallocation on chip type change
- Memory size tracked in `memory_size` field (32-bit for chips up to 64KB)

#### Read/Write/Erase Operations

- **Read operation**: Now reads entire EEPROM regardless of size (128B to 64KB)
- **Write operation**: Supports writing to full address range of selected chip
- **Erase operation**: Clears entire memory of selected chip type
- **File operations**: Binary dumps now save/load full chip capacity

#### User Interface Improvements

- Address display format adapts to memory size:
  - Small chips (≤256B): `0x00` format
  - Large chips (>256B): `0000` hex format (4 digits)
- Progress indicators updated for all memory sizes
- Navigation (Up/Down) works across entire address range
- File size display shows actual chip capacity

#### File Naming

- Filename generation now includes all chip types:
  - Examples: `24C01_2026-03-11_10-30.bin`, `24C256_2026-03-11_10-30.bin`
- Automatic timestamp-based naming for all chip variants

### 🔧 Technical Changes

#### Type Updates

- Changed address/size types from `uint8_t` to `uint32_t` for large memory support:
  - `current_address`: now `uint32_t`
  - `read_total_bytes`: now `uint32_t`
  - `write_total_bytes_async`: now `uint32_t`
  - `verify_total_bytes`: now `uint32_t`
  - `erase_current_addr`: now `uint32_t`
  - `progress_value`: now `uint32_t`
  - `file_size`: now `uint32_t`

#### Format Specifiers

- Updated all `printf`/`snprintf` calls to use correct format for `uint32_t`:
  - Changed `%d` to `%lu` for unsigned long
  - Changed `%X` to `%lX` for hex unsigned long

#### Memory Safety

- Added proper memory initialization in `reallocate_buffers()`
- Added null pointer checks for all dynamically allocated buffers
- Proper cleanup in `eeprom_app_free()` - all buffers freed correctly

### 🐛 Bug Fixes

- Fixed buffer overflow risk in memory operations for larger chips
- Fixed format specifier warnings causing compilation errors
- Fixed address boundary checking for chips larger than 256 bytes
- Fixed progress bar calculations for larger memory sizes

### 🔄 Behavioral Changes

- Settings → Chip Type now immediately reallocates buffers
- Current address is reset to 0 if it exceeds new chip size after type change
- File load operation respects maximum chip capacity (won't load more than chip can hold)

### 📋 Supported Chip Types

Complete support matrix:

| Chip Type | Size      | Status          |
|-----------|-----------|-----------------|
| 24C01     | 128 bytes | ✅ Full Support |
| 24C02     | 256 bytes | ✅ Full Support |
| 24C04     | 512 bytes | ✅ Full Support |
| 24C08     | 1 KB      | ✅ Full Support |
| 24C16     | 2 KB      | ✅ Full Support |
| 24C32     | 4 KB      | ✅ Full Support |
| 24C64     | 8 KB      | ✅ Full Support |
| 24C128    | 16 KB     | ✅ Full Support |
| 24C256    | 32 KB     | ✅ Full Support |
| 24C512    | 64 KB     | ✅ Full Support |

### ⚠️ Breaking Changes

- Binary dump files from previous versions (always 256 bytes) are incompatible with chip-specific sizes
- Users should re-read and save new dumps after upgrading

---

## [1.0.0] - Previous Version

### Initial Release

- Basic read/write/erase operations
- Fixed 256-byte buffer (24C02 only)
- I2C address configuration
- File load/save operations
- Basic hex viewer
