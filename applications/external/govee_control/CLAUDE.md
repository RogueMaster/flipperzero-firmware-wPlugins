# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Flipper Zero Bluetooth Govee Smart LED Controller - A native Flipper Zero application for controlling Govee BLE-enabled LED devices.

## Development Commands

### Build System
```bash
# Build the application (WORKS)
ufbt                          # Builds to dist/govee_control.fap

# Deploy and launch on connected Flipper (WORKS)
ufbt launch                   # Installs and launches app on device

# Flash firmware to device (REQUIRES DEVICE)
ufbt flash                    # Only works with Flipper connected

# Debug session (REQUIRES DEVICE)
ufbt debug                    # Needs connected Flipper

# Access CLI for logs (REQUIRES DEVICE)
ufbt cli                      # Serial connection to Flipper
```

### Code Quality Tools
```bash
# Run all code quality checks at once
./check.sh

# Format code with clang-format
clang-format -i govee_control/*.c govee_control/*.h

# Run static analysis with clang-tidy (requires LLVM)
clang-tidy govee_control/*.c -- -I. -Igovee_control

# Run bug detection with cppcheck
cppcheck --enable=all --suppress=missingInclude govee_control/

# Check formatting without modifying files
clang-format --dry-run --Werror govee_control/*.c
```

#### Installed Linting Infrastructure
- **clang-format** - Enforces consistent code style (LLVM-based, 100 char line limit)
- **clang-tidy** - Deep static analysis for bugs, memory issues, and best practices
- **cppcheck** - Additional bug detection focusing on memory safety
- **.clang-format** - Project formatting rules
- **.clang-tidy** - Static analysis configuration
- **compile_flags.txt** - Strict compiler warnings (-Wall -Wextra -Werror and more)
- **check.sh** - Automated script that runs all checks and reports issues

**IMPORTANT**: Always run `./check.sh` before committing to ensure code quality and catch potential bugs early.

### Project Structure
```
govee_control/
├── application.fam          # App manifest file
├── govee_control.c         # Main entry point
├── ble/                    # BLE communication layer
│   ├── ble_manager.c       # Connection management
│   ├── ble_scanner.c       # Device discovery
│   └── ble_protocol.c      # Protocol implementation
├── devices/                # Device-specific drivers
│   ├── device_registry.c   # Device abstraction layer
│   └── govee_h*.c         # Model-specific implementations
├── ui/                     # User interface
│   ├── views/              # UI screens
│   └── govee_ui.c         # UI manager
└── storage/                # Persistent storage
```

## Architecture

### Core Components
1. **BLE Manager** - Handles device discovery, connections, and command queuing
2. **Device Abstraction Layer** - Model-specific drivers for different Govee devices  
3. **Scene Engine** - Manages scenes, effects, and transitions
4. **Storage System** - Persistent storage for device configs and scenes

### BLE Protocol Implementation

#### Service and Characteristic UUIDs
```c
// Primary Control Service
#define GOVEE_SERVICE_CONTROL       0x000102030405060708090a0b0c0d1910
// Control Write Characteristic
#define GOVEE_CHAR_CONTROL_WRITE    0x000102030405060708090a0b0c0d2b10
// Status Read/Notify Characteristic  
#define GOVEE_CHAR_STATUS_READ      0x000102030405060708090a0b0c0d2b11
```

#### Packet Structure (20 bytes)
```
[HEADER][CMD][DATA.....................][CHECKSUM]
   0x33  0xXX  [17 bytes of data]         XOR
```

#### Key Commands
- **Power Control**: `0x33 0x01 [0x01=ON/0x00=OFF] ... [XOR]`
- **Brightness**: `0x33 0x04 [LEVEL: 0x00-0xFE] ... [XOR]`
- **RGB Color**: `0x33 0x05 0x02 [R] [G] [B] ... [XOR]`
- **Keep-Alive**: `0xAA 0x01 0x00 ... [XOR]` (send every 2 seconds)

#### Checksum Calculation
XOR all bytes 0-18 to generate byte 19

## Supported Devices
- **H6006** - Smart A19 LED Bulb (Priority)
- **H6160** - LED Strip Lights
- **H6163** - LED Strip Lights Pro
- **H6104** - LED TV Backlight  
- **H6110** - Smart Bulb
- **H6135** - Smart Light Bar
- **H6159** - Gaming Light Panels
- **H6195** - Immersion Light Strip

## Performance Requirements
- Command latency: <100ms
- Memory usage: <256KB RAM
- Storage: <1MB
- Battery impact: <10% per hour active use
- Simultaneous connections: 5+ devices

## Implementation Phases
1. **Phase 1 (Current)**: BLE scanner, basic connection, on/off control
2. **Phase 2**: Color/brightness control, multi-device support, H6006 full support
3. **Phase 3**: Effects library, animations, scene engine, scheduling
4. **Phase 4**: Performance optimization, battery improvements, testing