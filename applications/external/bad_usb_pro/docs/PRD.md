# PRD: BadUSB Pro — Advanced HID Payload Engine (`badusb_pro`)

## Overview
An enhanced USB HID keystroke injection engine for the Flipper Zero, compatible
with DuckyScript 2.0/3.0 syntax, featuring bidirectional communication via
keyboard LED state, conditional execution, variables, loops, and OS fingerprinting.

## Problem Statement
The stock Flipper BadUSB app is a basic DuckyScript 1.0 runner — it types
keystrokes and that's it. Real-world USB HID attacks need:
- **Feedback**: Know if a command succeeded before continuing
- **Conditionals**: Different payloads for Windows vs macOS vs Linux
- **Variables**: Store runtime data, build strings dynamically
- **Reliability**: Timing that adapts to target system responsiveness

Existing tools that do this (Rubber Ducky 3.0, O.MG Cable) cost $70-$200.
The Flipper already has all the hardware — it just needs better software.

## Target Users
- Red teamers performing USB drop / physical access attacks
- Security awareness trainers demonstrating USB risks
- Pentesters who need reliable, adaptive HID payloads

## Hardware Utilized
- **USB HID** interface via `furi_hal_usb_hid` (keyboard + mouse + consumer)
- **Keyboard LED state** via `furi_hal_hid_get_led_state()` — the feedback channel
- **USB CDC** can coexist for dual-mode operation
- **BLE HID** profile as alternative delivery (wireless)

## Core Features

### F1 — DuckyScript 3.0 Compatible Parser
Full command support:

| Category     | Commands                                           |
|-------------|-----------------------------------------------------|
| Basic        | `STRING`, `STRINGLN`, `DELAY`, `REM`               |
| Keys         | All HID keys, `GUI`, `ALT`, `CTRL`, `SHIFT`        |
| Combos       | `CTRL ALT DELETE`, `GUI r`, etc.                    |
| Mouse        | `MOUSE_MOVE x y`, `MOUSE_CLICK LEFT/RIGHT/MIDDLE`  |
| Mouse        | `MOUSE_SCROLL delta`                                |
| Flow         | `IF`, `ELSE`, `END_IF`, `WHILE`, `END_WHILE`       |
| Variables    | `VAR $name = value`, `$name` substitution           |
| Functions    | `FUNCTION name`, `END_FUNCTION`, `CALL name`        |
| LED Channel  | `LED_CHECK CAPS/NUM/SCROLL`, `LED_WAIT state`      |
| OS Detect    | `OS_DETECT` (sets `$OS` to WIN/MAC/LINUX/UNKNOWN`) |
| Timing       | `DEFAULT_DELAY ms`, `DEFAULT_STRING_DELAY ms`       |
| Control      | `REPEAT n`, `STOP`, `RESTART`                       |
| Consumer     | `CONSUMER_KEY key` (brightness, volume, etc.)        |

### F2 — LED Feedback Channel
The killer feature. Three keyboard LEDs = 3-bit feedback channel:

```
Host-side script toggles Caps/Num/Scroll Lock to signal the Flipper:
  - Caps Lock ON   = "command succeeded"
  - Num Lock ON    = "ready for next step"
  - Scroll Lock ON = "error occurred"

DuckyScript commands:
  LED_WAIT CAPS ON         # Block until Caps Lock turns on
  LED_CHECK NUM             # Set $LED_NUM to 0 or 1
  IF $LED_NUM == 1
    STRING Command succeeded
  END_IF
```

This enables payloads that *adapt* to what's happening on the target.

### F3 — OS Fingerprinting
`OS_DETECT` command uses timing analysis:
1. Press Caps Lock, measure time until LED state changes
2. Windows: ~30-60ms, macOS: ~10-20ms, Linux: varies by DE
3. Also checks: GUI key behavior, specific key combos
4. Sets `$OS` variable for conditional branching

### F4 — Script Management
- File browser for scripts on SD card (`/ext/badusb_pro/scripts/`)
- Recent scripts list (last 10)
- Script info view: name, size, estimated runtime, command count
- Syntax validation before execution

### F5 — Execution UI
- Live progress: current line / total lines
- Current command display
- LED state indicators (visual representation of Caps/Num/Scroll)
- Pause / Resume / Abort controls
- Speed adjustment (0.5x / 1x / 2x / 4x)

### F6 — Payload Library
Bundled example payloads (saved to SD on first launch):
- `windows_reverse_shell.ds` — PowerShell reverse shell with error handling
- `macos_terminal_exec.ds` — macOS Terminal execution with Gatekeeper bypass
- `linux_bash_exec.ds` — Linux terminal execution across common DEs
- `os_agnostic_exfil.ds` — Cross-platform file exfil via LED channel
- `wifi_password_grab.ds` — Extract saved WiFi passwords (Win/Mac/Linux)

### F7 — BLE HID Mode
- Toggle between USB HID and BLE HID delivery
- Same scripts work on both transports
- BLE mode useful for phones/tablets that don't have USB-A ports

## Architecture
```
main ──► SceneManager + ViewDispatcher
           ├── file_browser (FileBrowser module)
           ├── script_info (Widget)
           ├── execution_view (Custom View)
           ├── settings (VariableItemList)
           └── payload_library (Submenu)

script_engine (runs in execution_view callbacks)
  ├── parser: tokenizes .ds file into command list
  ├── interpreter: executes commands with variable scope
  ├── hid_driver: abstracts USB vs BLE HID
  └── led_monitor: polls LED state in background thread
```

## Key API Calls
```c
// USB HID
furi_hal_hid_kb_press(key);
furi_hal_hid_kb_release(key);
furi_hal_hid_get_led_state();  // Returns bitmask: Num|Caps|Scroll

// Mouse
furi_hal_hid_mouse_move(dx, dy);
furi_hal_hid_mouse_press(button);
furi_hal_hid_mouse_scroll(delta);

// BLE HID
ble_profile_hid_kb_press(profile, key);
ble_profile_hid_mouse_move(profile, dx, dy);

// USB mode switching
furi_hal_usb_set_config(&usb_hid, NULL);
```

## Security Notes
- Example payloads are clearly documented as for authorized testing only
- No built-in C2 infrastructure — payloads are self-contained scripts
- All payloads human-readable on SD card — no obfuscation

## Out of Scope
- USB mass storage emulation (not supported by HAL)
- Network-based C2 communication
- Automated payload generation

## Success Criteria
- Parses and executes standard DuckyScript 2.0 files without modification
- LED feedback channel works reliably on Windows 10/11
- OS detection correctly identifies Windows/macOS/Linux > 80% of the time
- BLE HID mode successfully injects keystrokes on iOS and Android
- Executes a 100-line payload in < 30 seconds (at 1x speed)
