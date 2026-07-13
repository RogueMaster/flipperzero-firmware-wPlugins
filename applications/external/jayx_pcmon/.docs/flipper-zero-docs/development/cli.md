<!-- Source: https://docs.flipper.net/zero/development/cli -->
<!-- Mirrored: 2026-07-14 -->

# Command-line interface - Flipper Zero Documentation

## Overview

The Flipper Zero command-line interface (CLI) enables users to control their device from a computer through text-based commands. According to the documentation, users can "read and emulate signals, run programs, manage files, and more on your flipper zero" through the CLI, with additional capabilities like viewing logs and communicating via sub-1 GHz radio.

## Accessing the CLI

Three methods are available:

**Method 1: Flipper Lab**
- Use Google Chrome or Chromium-based browsers supporting Web Serial API
- Visit lab.flipper.net
- Quit qFlipper and connect the device via USB

**Method 2: Web Serial Terminal**
- Access googlechromelabs.github.io/serial-terminal/
- Set baud rate to 230400
- Select Flipper Zero from the device list

**Method 3: Serial Terminal (OS-Specific)**

**macOS:**
- Find serial port: `ls /dev/cu`
- Connect via screen: `screen /dev/cu.usbmodemflip[name]`
- Or use minicom (install via Homebrew if needed)

**Linux:**
- Locate port: `ls /dev/serial/by-id/`
- Connect with screen or minicom using the full device path
- Alternative: try `/dev/ttyACM0` if other methods fail

**Windows:**
- Install PuTTY application
- Check Device Manager for COM port number
- Select Serial connection type, enter COM port, set speed to 230400

## Available Commands

The CLI provides numerous commands organized by function:

**System & Device Management**
- `info` - Display device and power information
- `power` - Control power functions (off, reboot, GPIO power)
- `date` - Display/set date and time
- `sysctl` - Configure system settings
- `uptime` - Show time since last reboot

**Signal & Communication**
- `ir` - Read/send infrared signals
- `rfid` - Read/emulate RFID data
- `nfc` - Read/emulate NFC cards
- `subghz` - Sub-GHz operations including chat feature
- `ikey` - iButton key operations
- `bt` - Bluetooth operations

**File & Storage**
- `storage` - File system operations (list, read, write, copy, delete)
- `update` - Manage firmware updates and backups

**Hardware Control**
- `gpio` - Direct GPIO pin control (read/write state)
- `led` - Control status LED and backlight brightness
- `vibro` - Activate/deactivate vibration motor
- `buzzer` - Generate sounds via piezo speaker

**Development & Diagnostics**
- `js` - Execute JavaScript files
- `log` - View system logs at various levels
- `loader` - Manage applications
- `crypto` - Encryption/decryption tools

## Logging

Users can monitor system activity by setting log levels:
- `log error` - Critical errors only
- `log warn` - Errors and warnings
- `log info` - Non-critical information (default)
- `log debug` - Debug information (impacts performance)
- `log trace` - System traces (affects performance)

Press Ctrl+C to stop logging.

## Sub-1 GHz Chat

To communicate with other Flipper Zero users:

```
subghz chat <frequency in hz> <device 0/1>
```

Frequency must be in allowed ranges (299999755-348000000, 386999938-464000000, or 778999847-928000000 Hz). Device parameter: 0 for internal antenna, 1 for external antenna.

## Detailed Command Categories

**NFC Shell Commands** include dumping card data, reading/writing MIFARE Ultralight tags, and sending raw protocols.

**RFID Operations** support multiple card types with specific data format requirements (EM4100, Indala, HID, etc.).

**Storage Commands** allow navigation and manipulation of internal (`/int`) and external (`/ext`) storage with operations like info, format, list, copy, and extraction.

**Infrared Functions** enable reading raw/decoded signals and sending commands using universal remotes (TV, audio, AC, projector).
