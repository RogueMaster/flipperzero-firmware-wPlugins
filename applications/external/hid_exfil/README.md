# HID Exfil

```
 _   _ ___ ____  _____       __ _ _
| | | |_ _|  _ \| ____|_  __/ _(_) |
| |_| || || | | |  _| \ \/ / |_| | |
|  _  || || |_| | |___ >  <|  _| | |
|_| |_|___|____/|_____/_/\_\_| |_|_|
```

**Keyboard LED covert channel data exfiltration for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-USB%20HID-yellow)](.)

> **For authorized security research, covert channel research, and educational use only.**

---

## What It Does

HID Exfil exploits the USB HID keyboard LED channel as a unidirectional covert data exfiltration path.

Normally, keyboard LEDs (CapsLock, NumLock, ScrollLock) are controlled by the host OS to reflect lock state. HID Exfil runs a small agent on the compromised host that encodes data into LED state change sequences. The Flipper Zero — connected as a USB HID keyboard — reads the LED feedback via the `furi_hal_hid_get_led_state()` API and reconstructs the data.

**Why this matters for security research:**
- LED state changes cross many DLP (Data Loss Prevention) monitoring boundaries
- The channel works even with network egress fully blocked
- No network connection, DNS, or process spawning required on the host
- Demonstrates a class of USB HID covert channels relevant to air-gapped environment assessments

**Throughput:** ~2–8 bps depending on encoding scheme and OS LED update latency. Suitable for small secrets (passwords, keys, tokens), not bulk data.

---

## Installation

**Build:**
```bash
cd hid_exfil
ufbt
```

**Deploy:**
```
dist/hid_exfil.fap  →  /ext/apps/USB/hid_exfil.fap
```

**Payloads:**
```
*.txt payload files  →  /ext/hid_exfil/
```

---

## How the Channel Works

### Encoding

Data is encoded as sequences of 3-bit values using the three LED lines:

| Bit | LED |
|---|---|
| Bit 0 | NumLock |
| Bit 1 | CapsLock |
| Bit 2 | ScrollLock |

Each state transition carries 3 bits. A framing protocol defines packet start/end and provides error detection.

### Protocol

```
Frame:
  [SYNC] [LEN:8b] [DATA:N*3b] [CRC:8b] [END]

SYNC = fixed pattern (toggle sequence unambiguous from normal typing)
LEN  = number of data bytes to follow
CRC  = XOR checksum over data bytes
```

The Flipper polls LED state after each keystroke it sends to the host. The host agent modulates LED state in response, encoding data in the transition sequence.

### Host Agent

The host-side agent is a minimal script that:
1. Reads the target data (file, environment variable, etc.)
2. Encodes it into the LED modulation sequence
3. Drives CapsLock/NumLock/ScrollLock via the OS HID LED output interface

Platform implementations:
- **Windows:** PowerShell using `[System.Windows.Forms.SendKeys]` and WinAPI `SetKeyboardState`
- **Linux:** `setleds` command or `/sys/class/leds/` interface
- **macOS:** IOKit `IOHIDSetModifierLockState`

The payload droppers in `hid_exfil_payloads.c` type the appropriate agent via USB HID into the target terminal before the exfiltration phase begins.

---

## Usage

### On Flipper

1. Plug Flipper Zero into the target machine via USB-C
2. Open `HID Exfil` from the USB apps menu
3. Select the data payload to receive (or enter receive mode)
4. Press **OK** to begin listening
5. The LED channel demodulates incoming data and displays it on screen
6. Optionally save received data to SD card

### Flipper Screen

```
┌─────────────────────────┐
│ HID Exfil               │
│ Mode: RECEIVE           │
│                         │
│ LED: N=0 C=0 S=0        │
│ Sync: waiting...        │
│                         │
│ [OK] Start [Back] Exit  │
└─────────────────────────┘
```

Once sync is detected:
```
┌─────────────────────────┐
│ HID Exfil               │
│ ████████░░░░ 67%        │
│ Bytes: 24 / 36          │
│                         │
│ Data: Sup3rS3cr3tP4ss!  │
└─────────────────────────┘
```

---

## Payload Configuration

Payloads are defined in `hid_exfil_payloads.c` and control what the agent types on the host machine. The agent can be configured to exfiltrate:

| Target | Description |
|---|---|
| `ENV_DUMP` | All environment variables (`PATH`, `USER`, tokens, secrets) |
| `WIFI_PSK` | Saved WiFi passwords from OS credential store |
| `CLIPBOARD` | Current clipboard contents |
| `FILE` | Arbitrary file path (configured via `FILE_PATH` option) |
| `CUSTOM` | User-defined command whose stdout is exfiltrated |

---

## Limitations

- **Throughput:** 2–8 bps. A 128-byte payload takes 2–8 minutes.
- **LED latency:** Some OS/driver combinations update LEDs slowly (>100ms). Reduce speed in settings.
- **Screen lock:** Agent cannot modulate LEDs if the screen is locked (host ignores HID input).
- **Virtual machines:** Guest OS LED state may not propagate to the Flipper correctly via hypervisor USB passthrough.

---

## SD Card Layout

```
/ext/
├── apps/
│   └── USB/
│       └── hid_exfil.fap
└── hid_exfil/
    └── received/
        └── exfil_<timestamp>.txt
```

---

## Legal Disclaimer

HID Exfil is for **authorized security research and covert channel assessment only.**
Exfiltrating data from systems you do not own or have explicit written permission to test is illegal.
