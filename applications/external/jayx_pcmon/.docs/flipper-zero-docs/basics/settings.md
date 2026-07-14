<!-- Source: https://docs.flipper.net/zero/basics/settings -->
<!-- Mirrored: 2026-07-14 -->

# Settings

## Overview

The settings application enables management of various Flipper Zero parameters, including "bluetooth connection, screen brightness, storage, and power management, as well as secure your device by setting a pin code."

## Main Settings Categories

### Bluetooth
Allows pairing with phones via the Flipper Mobile App and connection to smartphones or computers as a remote.

### LCD and Notifications
Manage display settings, LED brightness, sound volume, and vibrations using left and right buttons.

### Storage
View internal storage and microSD card usage, unmount/format cards, run benchmarks, and reset to factory settings. The microSD benchmark tests SPI interface performance (approximately 600 KiB/s reading speed recommended minimum write speed: 260 KiB/s).

### Power
Check battery information, reboot in normal or recovery mode, and power off the device.

### Desktop Settings

**PIN Setup:** Set a PIN code to protect the device. Access via Main Menu > Settings > Desktop > Pin Setup. Forgotten PINs can be reset by holding Up and Back buttons for 30 seconds.

**Show Clock:** Display a clock on the desktop.

**Quick Access Apps:** Assign up to four apps to pad buttons for direct desktop access.

**Happy Mode:** Keeps the digital pet dolphin always happy.

### System
- Switch between right-handed and left-handed modes
- Choose measurement units and time/date formats
- Enable debug mode and advanced features
- Toggle power saving mode (default: enabled)
- Adjust file naming conventions

### Sleep Method
Two modes available:
- **Default:** ~1.5 mA consumption, longer battery life
- **Legacy:** ~9 mA consumption, more stability

### Log Levels
Filter messages by severity (None, Error, Warning, Info, Debug, Trace). Output via UART pins 13/14 (default) or 15/16.

### Debug Mode
Provides additional debugging functionality in Sub-GHz, RFID, NFC, and infrared applications, plus CLI commands and SWD interface access.

### Heap Trace
Records memory allocation/deallocation for debugging memory-related issues.

### Expansion Modules
Select UART hardware for external module communication (USART, LPUART, or None).

### Clock & Alarm
Adjust date, time, and timezone; set alarms.

### About
General device information, unique identifiers, hardware details, region, and firmware information.

## Quick Access to Settings

Add frequently-used settings to favorites by selecting a setting, holding OK, and pressing "Pin" for desktop access.
