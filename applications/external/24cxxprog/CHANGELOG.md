# Changelog

All notable changes to the 24cxxprog EEPROM Programmer application will be documented in this file.

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
| Chip Type | Size | Status |
|-----------|------|--------|
| 24C01 | 128 bytes | ✅ Full Support |
| 24C02 | 256 bytes | ✅ Full Support |
| 24C04 | 512 bytes | ✅ Full Support |
| 24C08 | 1 KB | ✅ Full Support |
| 24C16 | 2 KB | ✅ Full Support |
| 24C32 | 4 KB | ✅ Full Support |
| 24C64 | 8 KB | ✅ Full Support |
| 24C128 | 16 KB | ✅ Full Support |
| 24C256 | 32 KB | ✅ Full Support |
| 24C512 | 64 KB | ✅ Full Support |

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
