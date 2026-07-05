# 🔧 24cxxprog - EEPROM 24Cxx Programmer

<h2 align="center">A Comprehensive EEPROM Programmer for Flipper Zero</h2>

<div align="center">
    <table style="width:100%; border:none;">
        <tr style="border:none;">
            <td style="border:none; padding:10px;">
                <img src="screenshots/1.png" alt="Main Menu - Operations" style="width:100%;">
                <br>
                <em>Splashscreen</em>
            </td>
            <td style="border:none; padding:10px;">
                <img src="screenshots/2.png" alt="Configuration Menu" style="width:100%;">
                <br>
                <em>Main menu</em>
            </td>
            <td style="border:none; padding:10px;">
                <img src="screenshots/3.png" alt="Data Display - EEPROM Contents" style="width:100%;">
                <br>
                <em>Displaying EEPROM Contents (Hex Data)</em>
            </td>
        </tr>
    </table>
</div>

[![FAP : Build and lint](https://github.com/kamylwnb/24cxxprog/actions/workflows/build.yml/badge.svg)](https://github.com/kamylwnb/24cxxprog/actions/workflows/build.yml)

---

This is a **comprehensive EEPROM programmer application** designed for the **Flipper Zero** that interfaces with the **24Cxx series I2C memory chips**. The application provides a complete suite of tools for reading, writing, erasing, and managing EEPROM memory with a user-friendly interface on the Flipper's screen.

## ✨ Features Overview

### 📝 EEPROM Operations

Complete toolset for memory management:

* **Read Operations:** View complete EEPROM contents with address and hexadecimal data display.
* **Write Operations:** Program custom data into specific memory addresses.
* **Erase Functions:** Clear individual bytes, pages, or entire memory sections.
* **Dump to Storage:** Export EEPROM contents to Flipper SD card for backup and analysis.
* **Restore from Backup:** Load previously saved EEPROM data back into the chip.

### 🎨 User Interface & Experience

Intuitive interface optimized for Flipper Zero's display:

* **Main Menu:** Clear operation selection with visual feedback.
* **Data Viewer:** Scrollable hex display showing actual EEPROM contents.
* **Configuration Menu:** Easy access to sensor parameters and device settings.
* **Address Navigation:** Precise control over memory location selection.
* **Progress Indicator:** Real-time feedback during long operations.

### ⚙️ Configuration Options

Customize the programmer for your specific hardware:

* **I2C Address Selection:** Choose between multiple I2C addresses (**0x50-0x57**) for different chip variants.
* **Memory Size Selection:** Automatically detect or manually set chip capacity (**1KB to 64KB** and larger).
* **Page Size Configuration:** Adapt to different chip architectures (**8 bytes to 256 bytes per page**).
* **Persistent Settings:** Configurations are automatically saved for quick access.

### 💻 Technical Features & Robustness

Built for reliability on the Flipper Zero platform:

* **I2C Protocol Support:** Robust communication with error checking.
* **Address Validation:** Prevents out-of-bounds memory access.
* **Timeout Protection:** Safeguards against communication errors.
* **Error Handling:** Comprehensive error messages for troubleshooting.
* **Non-blocking Operations:** Responsive UI that doesn't freeze during I2C transactions.
* **Data Verification:** Verify written data integrity after programming.

## 🔋 Supported 24Cxx Chips

Comprehensive support for the entire 24Cxx family:

| Chip Model          | Memory Size | Page Size | Address Range   |
|---------------------|-------------|-----------|-----------------|
| **24C01**           | 128 Bytes   | 8 Bytes   | 0x00 - 0x7F     |
| **24C02**           | 256 Bytes   | 8 Bytes   | 0x00 - 0xFF     |
| **24C04 - 24C16**   | 512B - 2KB  | 16 Bytes  | 0x00 - 0xFFFF   |
| **24C32 - 24C64**   | 4KB - 8KB   | 32 Bytes  | 0x0000 - 0x1FFF |
| **24C128 - 24C512** | 16KB - 64KB | 64 Bytes  | 0x0000 - 0xFFFF |

---

## 🕹️ Navigation Guide

| Screen            | D-Pad Up/Down                                         | D-Pad Left/Right                | OK Button             | Back Button          |
|-------------------|-------------------------------------------------------|---------------------------------|-----------------------|----------------------|
| **Main Menu**     | Browse operations (Read, Write, Erase, Dump, Restore) | -                               | Select operation      | **Exit** application |
| **Read/Write**    | Navigate through addresses                            | Adjust byte values (Write mode) | Confirm operation     | Return to Main Menu  |
| **Configuration** | Navigate between settings                             | Adjust parameter values         | Apply settings        | Cancel and return    |
| **Data View**     | Scroll data up/down                                   | Jump to address                 | Show hex/ASCII toggle | Exit data view       |

## 🔌 Hardware Connections

Standard I2C pinout for Flipper Zero GPIO:

```txt
24Cxx EEPROM Module      Flipper Zero GPIO
─────────────────        ─────────────────
SDA (Pin 5)       ───→   GPIO_SDA (Pin 15)
SCL (Pin 6)       ───→   GPIO_SCL (Pin 16)
GND (Pin 4)       ───→   GND (Pin 8)
VCC (Pin 8)       ───→   3.3V (Pin 9)

Optional Pull-ups: 4.7kΩ from SDA and SCL to 3.3V
```

## 📋 Operation Details

### Read

* Displays EEPROM contents in hexadecimal format
* Shows address, data bytes, and ASCII representation
* Scrollable for chips larger than display capacity

### Write

* Enter target address and data values
* Supports single byte or page programming
* Automatic write cycle delay handling

### Erase

* Clear individual bytes to 0xFF
* Erase entire pages
* Full chip erase with confirmation

### Dump

* Export EEPROM to **`/ext/apps_data/24cxxprog/`** directory
* Creates timestamped backup files
* Preserves complete memory state

### Restore

* Load previously dumped EEPROM data
* Verify before writing
* Restore to specified starting address

---

## 👨‍💻 Developer

This application was created by **Dr. Mosfet** for the Flipper Zero community.

**Repository:** [kamylwnb/24cxxprog](https://github.com/kamylwnb/24cxxprog)

**Version:** 1.0  
**Category:** GPIO / Tools  
**Platform:** Flipper Zero F7

---

**Happy EEPROM programming! 🔧**
