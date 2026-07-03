# BadUSB Pro

```
 ____            _  _   _ ____  ____    ____
| __ )  __ _  __| || | | / ___|| __ )  |  _ \ _ __ ___
|  _ \ / _` |/ _` || | | \___ \|  _ \  | |_) | '__/ _ \
| |_) | (_| | (_| || |_| |___) | |_) | |  __/| | | (_) |
|____/ \__,_|\__,_| \___/|____/|____/  |_|   |_|  \___/
```

**DuckyScript 3.0 keystroke injection engine for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-USB%20HID-yellow)](.)

> **For authorized security testing and educational use only.**

---

## What It Does

BadUSB Pro turns Flipper Zero into a USB HID keyboard that executes DuckyScript 3.0 payloads against a connected computer. Unlike the Flipper's built-in BadUSB app, BadUSB Pro adds:

- **OS auto-detection** — fingerprints Windows / macOS / Linux via USB LED heuristics before running any keystrokes
- **DuckyScript 3.0** — variables, conditionals (`IF/END_IF`), `OS_DETECT`, `DEFAULT_DELAY`, `STRINGLN`, and more
- **LED feedback** — Flipper's RGB LED indicates current state (detecting, running, done, error)
- **Script library** — loads `.ds` scripts from `/ext/badusb_pro/` on the SD card

---

## Installation

**Build:**
```bash
cd badusb_pro
ufbt
```

**Deploy:**
```
dist/badusb_pro.fap  →  /ext/apps/USB/badusb_pro.fap
```

**Scripts:**
```
*.ds files  →  /ext/badusb_pro/
```

---

## Script Format (DuckyScript 3.0)

Scripts are plain text files with the `.ds` extension.

### Core Commands

| Command | Description |
|---|---|
| `REM <text>` | Comment — ignored |
| `STRING <text>` | Type text via HID |
| `STRINGLN <text>` | Type text + press Enter |
| `DELAY <ms>` | Wait N milliseconds |
| `DEFAULT_DELAY <ms>` | Set delay between all subsequent commands |
| `ENTER` / `RETURN` | Press Enter |
| `TAB` | Press Tab |
| `ESCAPE` / `ESC` | Press Escape |
| `BACKSPACE` | Backspace |
| `DELETE` | Forward delete |
| `HOME` / `END` | Home / End |
| `PAGEUP` / `PAGEDOWN` | Page Up / Down |
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Arrow keys |
| `F1` … `F12` | Function keys |

### Modifier Combinations

| Command | Keys |
|---|---|
| `GUI <key>` / `WINDOWS <key>` | Win key + key |
| `COMMAND <key>` | Cmd (macOS) + key |
| `CTRL <key>` | Ctrl + key |
| `ALT <key>` | Alt + key |
| `SHIFT <key>` | Shift + key |
| `CTRL ALT <key>` | Ctrl + Alt + key |

### DuckyScript 3.0 Extensions

| Command | Description |
|---|---|
| `OS_DETECT` | Auto-detect OS and set `$OS` variable (`WIN`, `MAC`, `LINUX`, `UNKNOWN`) |
| `IF $OS == WIN` | Conditional block — execute if OS matches |
| `END_IF` | End conditional block |
| `VAR $NAME = value` | Declare a variable |
| `STRING $NAME` | Type the value of a variable |

---

## OS Detection

BadUSB Pro uses a single-phase USB LED timing heuristic — no visible keystrokes reach the screen during detection:

1. Toggle Caps Lock and measure how quickly the host OS reflects the LED state change back via USB HID
2. Classify by response latency: ≤25ms = macOS, ≤70ms = Windows, >70ms = Linux
3. Toggle Caps Lock back to restore the original state

The result is stored in the `$OS` variable (`WIN`, `MAC`, `LINUX`, or `UNKNOWN` if no response within 500ms).

---

## Sample Scripts

Three sample scripts ship in `badusb_pro_sample_scripts/`:

### `hello_world.ds`
Opens Notepad and types a demonstration message. Good for confirming the setup works.

### `os_detect_demo.ds`
Demonstrates OS detection — opens the appropriate text editor on each platform and types the detected OS name.

### `led_feedback_demo.ds`
Shows LED state changes throughout a script execution. Useful for building feedback into your own scripts.

---

## Writing Custom Scripts

```
REM My custom payload
REM For authorized testing only

DEFAULT_DELAY 100

OS_DETECT

IF $OS == WIN
  GUI r
  DELAY 500
  STRING powershell -w h -nop
  ENTER
  DELAY 800
  STRING whoami
  ENTER
END_IF

IF $OS == MAC
  COMMAND SPACE
  DELAY 400
  STRING terminal
  ENTER
  DELAY 800
  STRING whoami
  ENTER
END_IF

IF $OS == LINUX
  CTRL ALT t
  DELAY 800
  STRING whoami
  ENTER
END_IF
```

**Tips:**
- Always add `DELAY` after opening dialogs and terminals — they need time to appear
- Use `OS_DETECT` at the top so each platform block runs correctly
- Test on your own machines before a live engagement
- Keep `DEFAULT_DELAY` at 80–120ms for reliable typing across all target hardware

---

## LED Status

| Color | Meaning |
|---|---|
| Blue (slow pulse) | Idle — waiting for USB host |
| Yellow (fast pulse) | OS detection in progress |
| Green | Script running |
| White flash | Keystroke sent |
| Red | Error (script not found, USB not connected) |
| Green solid | Script complete |

---

## SD Card Layout

```
/ext/
├── apps/
│   └── USB/
│       └── badusb_pro.fap
└── badusb_pro/
    ├── hello_world.ds
    ├── os_detect_demo.ds
    ├── led_feedback_demo.ds
    └── your_script.ds
```

---

## Legal Disclaimer

BadUSB Pro is for **authorized security testing and educational use only.**
Using this against systems you do not own or have explicit written permission to test is illegal under the CFAA and equivalent laws worldwide.
