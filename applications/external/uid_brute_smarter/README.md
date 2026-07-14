# UID Brute Smarter 🔐

> An advanced NFC UID analysis and testing tool for the Flipper Zero.

[![Build Status](https://github.com/fbettag/uid_brute_smarter/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/fbettag/uid_brute_smarter/actions/workflows/test.yml)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-Compatible-green.svg)](https://flipperzero.one/)

## 🎯 Overview

UID Brute Smarter is a security research tool for authorized penetration testing and access control system auditing. It provides advanced NFC key management and intelligent pattern detection to build and execute comprehensive security assessments.

> ⚠️ **IMPORTANT**: This tool is intended for **authorized security testing only**. Users must obtain proper authorization before testing any systems they do not own or have explicit permission to test.

## ✨ Features

### 🔍 Key Management
- **NFC File Loading**: Load NFC files with metadata extraction.
- **Key Tracking**: Per-key metadata including filename, UID, and load time.
- **Key Browser**: Browse and manage loaded keys with UID display.
- **Bulk Operations**: Load multiple keys or unload all at once.
- **Memory Safety**: Proper cleanup and memory management.

### 🧠 Pattern Detection
- **Advanced Algorithms**: Detects +1, +K, 16-bit counter, and bitmask patterns.
- **Range Generation**: Automatically creates test ranges based on detected patterns.
- **Pattern Validation**: Ensures generated ranges are within safe bounds.
- **Configurable Limits**: Prevents excessive range generation.

### ⚙️ Configuration
- **Adjustable Timing**: Configurable delays between attempts (100ms-1000ms).
- **Pause Management**: Set pause intervals to prevent system overload.
- **Progress Tracking**: Real-time progress display during testing.
- **Safe Stopping**: Graceful interruption capabilities.

### 🎛️ User Interface
- **Intuitive Interface**: Clean GUI following Flipper design principles.
- **Detailed Feedback**: Clear notifications for all operations.
- **Error Handling**: Comprehensive validation and user feedback.

## 🚀 Quick Start

## 📖 Usage Guide

1. **Authorization**: Ensure you have **explicit written authorization** for target systems.
2. **Key Collection**: Load authorized test cards (`.nfc` files) from the `/ext/nfc/` folder.
3. **Configuration**: Adjust delay and pause settings as needed.
4. **Key Management**: View, manage, or unload keys.
5. **Testing**: Start the brute-force attack and monitor progress.

## 📊 Technical Specifications

### Pattern Detection
- **+1 Linear**: Sequential incrementing patterns.
- **+K Linear**: Fixed step patterns (16, 32, 64, 100, 256).
- **16-bit Counter**: Little-endian counter patterns.
- **Bitmask**: Patterns where only a few bits are changing.
- **Unknown**: Safe range expansion around provided keys.

### Supported Formats
- **NFC Files**: Standard `.nfc` files with ISO14443-3a data.
- **UID Length**: 4-byte UIDs.

## 🤝 Support

- **Issues**: [GitHub Issues](../../issues) for bug reports.
- **Discussions**: [GitHub Discussions](../../discussions) for questions.

## 🙏 Credits

- **[Momentum Firmware](https://momentum-fw.dev/)**
- **[Unleashed Firmware](https://github.com/DarkFlippers/unleashed)**
- **[Flipper Zero Community](https://github.com/flipperdevices)**

## 📄 License

This project is licensed under the **BSD 3-Clause License**.
