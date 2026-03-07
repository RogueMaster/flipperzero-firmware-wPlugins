# Evil Portal

```
 _____     _ _  ____            _        _
| ____|_ _(_) ||  _ \ ___  _ __| |_ __ _| |
|  _| \ V / | || |_) / _ \| '__| __/ _` | |
| |___ \ /| | ||  __/ (_) | |  | || (_| | |
|_____/_/ |_|_||_|   \___/|_|   \__\__,_|_|
```

**Captive portal toolkit for WiFi Marauder on ESP32 + Flipper Zero**

> **For authorized security testing, WiFi pentesting engagements, and educational use only.**
> Deploying fake portals against users you do not have written permission to test is illegal.

---

## What Is This?

Evil Portal is a feature in [WiFi Marauder](https://github.com/justcallmekoko/ESP32Marauder) firmware that runs a rogue access point with a DNS spoofing server and HTTP captive portal on the ESP32. When a victim device connects, all DNS queries resolve to `172.0.0.1`, where Marauder serves your chosen HTML page and logs any form submissions to the SD card.

This directory contains:

| Path | Contents |
|---|---|
| `portals/` | 9 ready-to-deploy HTML captive portal pages |
| `marauder_scripts/fpwn/` | Novel `.fpwn` modules for FlipperPwn (multi-phase WiFi attack chains) |
| `marauder_scripts/serial_tools/` | Python utilities for PC-side serial control and PCAP capture |

---

## Portal Pages

### Common Scenarios

| File | Scenario | Captures |
|---|---|---|
| `router_generic.html` | Router admin session expiry | Username + password |
| `hotel_wifi.html` | Hotel guest WiFi with loyalty login | Last name, room, email, password |
| `coffee_shop.html` | Free coffee shop WiFi email opt-in | Name + email |
| `corporate_guest.html` | Enterprise guest network authentication | Email, access code, host name |
| `airport_wifi.html` | Airport free WiFi identity verification | Email or phone + flight number |

### Unique / Novel Scenarios

| File | Scenario | Why It Works |
|---|---|---|
| `isp_maintenance.html` | ISP account verification during "outage" | Urgency + account number + password combo |
| `smart_home_setup.html` | Smart home hub WiFi provisioning | Victims expect to enter WiFi credentials here |
| `emergency_portal.html` | Security incident identity verification | High-urgency social engineering for corporate targets |

### Trekistry Branded

| File | Purpose |
|---|---|
| `trekistry_escape.html` | Safety-awareness page: warns victim they connected to an unofficial AP, promotes Trekistry as a travel platform. No credential capture — redirect/awareness use. |

---

## Deploying a Portal (Marauder CLI)

**Step 1 — Copy your HTML to the ESP32 SD card:**
```
SD card root:
  /portals/
    hotel_wifi.html
    corporate_guest.html
    ...
```

**Step 2 — Scan for APs and pick your target:**
```
scanap
```

**Step 3 — Set the HTML page:**
```
evilportal -c sethtml hotel_wifi.html
```

**Step 4 — Optionally clone a specific AP's SSID:**
```
evilportal -c setap 2
```
(where `2` is the AP index from `scanap`)

**Step 5 — Start the portal:**
```
evilportal -c start
```

**Step 6 — Monitor captures:**
Credentials appear on the Flipper display in real time and are logged to:
```
/logs/evil_portal.log
```

**Step 7 — Stop:**
```
stopscan
```

---

## FlipperPwn Modules (`.fpwn`)

These modules require [FlipperPwn](../flipperpwn/) and an ESP32 WiFi Dev Board.
Copy them to your Flipper SD card under `/ext/flipperpwn/modules/credential/`.

| File | Description |
|---|---|
| `evil_twin.fpwn` | Scan → deauth target AP clients → deploy evil portal with cloned SSID |
| `probe_karma_portal.fpwn` | Sniff probe requests → identify top-probed SSID → targeted karma + portal |
| `pmkid_harvest.fpwn` | Dual-vector: PMKID hash capture + parallel evil portal (hash + cleartext) |
| `wifi_survey_report.fpwn` | Full WiFi recon survey auto-typed as formatted report via USB HID |
| `ble_chaos.fpwn` | Sequential BLE attack coverage: iOS / SwiftPair / Samsung / AirTag |

### What Makes These Novel

Most public Marauder attack chains do one thing at a time. These modules chain multiple phases:

- **`probe_karma_portal`** — First passively identifies what SSIDs devices *want* to connect to, then targets only the highest-demand one. More stealthy and targeted than broad karma attacks.

- **`pmkid_harvest`** — Runs PMKID capture (offline-crackable WPA2 hash) and evil portal (direct cleartext) simultaneously. Whichever vector succeeds first wins.

- **`wifi_survey_report`** — Turns Flipper Zero into an automated wireless survey tool that types a formatted pentest report directly into any text editor via USB HID. No software installation on target required.

---

## Python Serial Tools

Located in `marauder_scripts/serial_tools/`. Require `pip install pyserial`.

### `marauder_serial.py` — Interactive Marauder Shell

```bash
python3 marauder_serial.py [PORT] [BAUD]
python3 marauder_serial.py /dev/ttyUSB0
python3 marauder_serial.py COM3 115200
```

Features:
- Auto-detects Flipper/ESP32 serial port
- Command history with readline
- Automatic PCAP frame extraction (saves `[BUF/BEGIN]..[BUF/CLOSE]` blocks as `.pcap` files)
- Built-in **macros** for common attack sequences:

| Macro | What It Does |
|---|---|
| `!macro quick_scan` | AP scan + station scan + list results |
| `!macro pmkid_force` | PMKID capture with forced deauth on ch 6 (60s) |
| `!macro deauth_flood` | Scan + select first AP + deauth flood |
| `!macro beacon_spam_random` | Random beacon spam |
| `!macro recon` | AP scan + probe sniff + beacon sniff |
| `!macro ble_full` | Full BLE attack sequence (scan → sourapple → swiftpair → samsung → airtag) |

```
marauder> !macros             # list macros
marauder> !macro recon        # run recon macro
marauder> scanap              # direct command
marauder> !pcap               # show saved PCAP files
marauder> !help               # full help
```

### `pcap_capture.py` — Dedicated PCAP Capture

```bash
python3 pcap_capture.py --mode sniffraw --duration 60 --out ./captures
python3 pcap_capture.py --mode sniffpmkid --channel 6 --deauth --duration 120
python3 pcap_capture.py --mode sniffbeacon
```

Features:
- Handles `[BUF/BEGIN]`/`[BUF/CLOSE]` binary framing
- Writes valid libpcap files (readable by Wireshark, tshark, Scapy)
- Live progress bar with frame counter
- Multiple captures in one session → separate timestamped `.pcap` files
- Graceful `Ctrl+C` shutdown with partial capture flush

```
Modes:
  sniffraw       All 802.11 frames
  sniffbeacon    Beacon frames
  sniffdeauth    Deauthentication frames
  sniffpmkid     PMKID/EAPOL handshake (use --deauth to force handshakes)
  sniffprobe     Probe requests
```

**Process captured PMKID hashes:**
```bash
# Convert to hashcat format
hcxpcapngtool -o hash.hc22000 capture_20260302_091400_001.pcap

# Crack with hashcat (dictionary)
hashcat -m 22000 hash.hc22000 /usr/share/wordlists/rockyou.txt

# Crack with hashcat (rules)
hashcat -m 22000 hash.hc22000 /usr/share/wordlists/rockyou.txt -r best64.rule
```

---

## Hardware Setup

```
Flipper Zero GPIO header
│
├── Pin 13 (TX) ──────────────► ESP32 RX0
├── Pin 14 (RX) ◄────────────── ESP32 TX0
├── Pin 9 (3.3V) ─────────────► ESP32 3.3V
└── Pin 8/11/18 (GND) ────────► ESP32 GND
```

The **official Flipper Zero WiFi Dev Board** plugs directly into the GPIO header with no wiring.
Third-party ESP32 boards require the connections above.

Flash WiFi Marauder: https://github.com/justcallmekoko/ESP32Marauder

---

## SD Card Layout

**ESP32 SD card:**
```
/
├── portals/
│   ├── hotel_wifi.html
│   ├── corporate_guest.html
│   └── ...
└── logs/
    ├── evil_portal.log
    ├── pmkid_20260302.pcap
    └── probes.log
```

**Flipper SD card (for FlipperPwn modules):**
```
/ext/flipperpwn/modules/credential/
├── evil_twin.fpwn
├── probe_karma_portal.fpwn
├── pmkid_harvest.fpwn
└── ...
```

---

## Legal Disclaimer

This toolkit is provided for **authorized security testing, WiFi pentesting engagements, and educational research only.**

- Do not deploy portals against networks or users you do not own or have explicit written permission to test
- Unauthorized interception of network credentials is a federal crime under the CFAA and equivalent laws worldwide
- The authors accept no liability for misuse
