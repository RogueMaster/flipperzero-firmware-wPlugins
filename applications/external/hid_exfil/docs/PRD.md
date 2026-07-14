# PRD: HID-Based Data Exfiltration Tool (`hid_exfil`)

## Overview
A USB HID attack tool that combines keystroke injection with the keyboard LED
feedback channel to exfiltrate small amounts of data from target systems. The
Flipper injects a script that reads data and encodes it as Caps Lock / Num Lock /
Scroll Lock toggles, which the Flipper decodes by polling `furi_hal_hid_get_led_state()`.

## Problem Statement
Air-gapped and heavily locked-down systems (no network, no USB storage, DLP
enabled) are considered safe from data exfiltration. But they almost always allow
USB keyboards. The keyboard LED feedback channel is a covert exfiltration path
that no DLP/EDR product monitors because:
- It's a hardware-level HID feature, not a software protocol
- LED state changes are handled by the USB HID driver in the kernel
- No data leaves via network — it's electrical signals on the USB bus

This technique is known in academic research but no fieldable tool exists.

## Target Users
- Red teamers demonstrating risk of USB keyboard attacks on air-gapped systems
- Pentesters proving data exfiltration from "secure" environments
- Security researchers studying covert channels

## Hardware Utilized
- **USB HID keyboard** via `furi_hal_usb_hid`
- **LED state polling** via `furi_hal_hid_get_led_state()` — reads Num/Caps/Scroll
- **SD card** for storing exfiltrated data
- **Display** for real-time progress

## Theory of Operation

### The LED Channel
USB HID spec defines 3 keyboard LEDs controlled by the host:
- **Num Lock** (bit 0)
- **Caps Lock** (bit 1)
- **Scroll Lock** (bit 2)

The Flipper can **read** these LED states. A script running on the target can
**set** them by sending the appropriate keystrokes. This creates a 3-bit
host→device channel at ~50-100 baud.

### Encoding Protocol
```
Each byte transmitted as follows:
1. Flipper sets Scroll Lock ON (via keystroke) = "ready to receive"
2. Target encodes byte as sequence of LED toggles:
   - 4 dibits (2-bit pairs) transmitted via Caps+Num Lock
   - Caps Lock = bit 1, Num Lock = bit 0
   - Scroll Lock toggled by target after each dibit = "clock"
3. Flipper reads dibit on each Scroll Lock transition
4. After 4 dibits (1 byte), cycle repeats

Effective throughput: ~20-30 bytes/second
```

## Core Features

### F1 — Injection Scripts (Host-Side Payloads)
Pre-built PowerShell/bash scripts that the Flipper types into the target.
These scripts:
1. Read specified data (file contents, registry keys, env vars, etc.)
2. Encode data as LED toggle sequences
3. Transmit via the LED channel

Available payloads:
| Payload                | OS      | Exfiltrates                          |
|------------------------|---------|--------------------------------------|
| `wifi_passwords`       | Windows | Saved WiFi SSIDs + passwords         |
| `env_vars`             | All     | Environment variables                |
| `clipboard`            | All     | Current clipboard contents           |
| `file_read`            | All     | Contents of a specified file path    |
| `registry_key`         | Windows | Value of a specified registry key    |
| `browser_bookmarks`    | All     | Chrome/Firefox bookmark URLs         |
| `ssh_keys`             | Linux   | SSH private key filenames (not keys) |
| `system_info`          | All     | Hostname, IP, OS version, users      |
| `custom`               | All     | User-provided script from SD card    |

### F2 — LED Receiver Engine
Background thread continuously polls LED state:
- Polling rate: 1000 Hz (1ms interval)
- Debounce: 5ms (ignore transitions shorter than 5ms)
- Synchronization: Scroll Lock toggle = clock edge
- Error detection: timeout if no clock edge for > 500ms
- Byte assembly: 4 dibits → 1 byte, accumulated into buffer

### F3 — Real-Time Progress Display
```
┌──────────────────────────────┐
│ HID Exfil - wifi_passwords   │
│                              │
│ Phase: Receiving data        │
│ Bytes: 1,247 / ~4,096       │
│ Rate:  23 B/s                │
│ Time:  0:52 elapsed          │
│                              │
│ LED: [N:●] [C:○] [S:●]     │
│                              │
│ [OK: Pause]    [Back: Abort] │
└──────────────────────────────┘
```

### F4 — Data Viewer
- View received data on-device (text mode, scrollable)
- Hex dump mode for binary data
- Save to SD card: `exfil_YYYYMMDD_HHMMSS.txt`
- Data persists across app restarts

### F5 — Custom Payload Editor
- Point to a custom script on SD card
- Script must implement the LED encoding protocol
- Template scripts provided for PowerShell and Bash

### F6 — Settings
- Target OS: Windows / macOS / Linux (affects injected script syntax)
- Injection speed: Slow (50ms/key) / Normal (20ms/key) / Fast (5ms/key)
- LED polling rate: 500Hz / 1000Hz / 2000Hz
- Open terminal command: Configurable per OS
- Post-exfil cleanup: Yes/No (clear command history, close terminal)
- Stealth mode: Minimize terminal window during operation

## Architecture
```
main ──► SceneManager + ViewDispatcher
           ├── payload_select (Submenu)
           ├── config (VariableItemList)
           ├── execution_view (Custom View — progress display)
           ├── data_viewer (TextBox)
           └── settings (VariableItemList)

exfil_worker (FuriThread)
  ├── phase 1: inject keystrokes to open terminal
  ├── phase 2: inject encoder script
  ├── phase 3: receive LED data in polling loop
  ├── phase 4: (optional) inject cleanup commands
  └── saves received data to FuriString buffer + SD card
```

## Key API Calls
```c
// Keystroke injection
furi_hal_hid_kb_press(HID_KEYBOARD_R);   // with GUI modifier for Win+R
furi_hal_hid_kb_release(HID_KEYBOARD_R);

// LED state reading (the exfil channel)
uint8_t leds = furi_hal_hid_get_led_state();
bool num_lock   = leds & (1 << 0);
bool caps_lock  = leds & (1 << 1);
bool scroll_lock = leds & (1 << 2);

// Toggle a lock key (to signal target)
furi_hal_hid_kb_press(HID_KEYBOARD_SCROLL_LOCK);
furi_hal_hid_kb_release(HID_KEYBOARD_SCROLL_LOCK);
```

## Protocol Specification
```
SYNC PHASE:
  Flipper toggles Scroll Lock 3 times rapidly (100ms each)
  Target script detects this pattern and begins transmission

DATA PHASE:
  For each byte B:
    Target sets Caps = (B >> 7) & 1, Num = (B >> 6) & 1
    Target toggles Scroll Lock (clock)
    Target sets Caps = (B >> 5) & 1, Num = (B >> 4) & 1
    Target toggles Scroll Lock (clock)
    Target sets Caps = (B >> 3) & 1, Num = (B >> 2) & 1
    Target toggles Scroll Lock (clock)
    Target sets Caps = (B >> 1) & 1, Num = (B >> 0) & 1
    Target toggles Scroll Lock (clock)

END PHASE:
  Target toggles all three LEDs simultaneously 3 times = "done"

ERROR:
  If no clock edge for 1 second, Flipper retries sync
  After 3 retries, abort and save partial data
```

## Security & Ethics
- Prominently marked as authorized penetration testing tool
- Warning screen on launch with "I have authorization" acknowledgment
- All injected scripts are visible/editable on SD card — no hidden behavior
- No built-in persistence mechanisms
- Cleanup mode removes evidence of the attack

## Out of Scope
- Network-based exfiltration (out of scope — this IS the network-free alternative)
- Large file exfiltration (practical limit ~10-50 KB due to speed)
- Encrypted channel (target script runs in plaintext anyway)
- Android/iOS targets (no keyboard LED control)

## Success Criteria
- Successfully exfiltrates WiFi passwords from Windows 10/11 air-gapped system
- Achieves sustained 20+ bytes/second throughput
- Zero data corruption over 4 KB transfer (verified by checksum)
- Complete operation (inject + receive + cleanup) in under 5 minutes for typical payload
- No artifacts visible on target screen in stealth mode
