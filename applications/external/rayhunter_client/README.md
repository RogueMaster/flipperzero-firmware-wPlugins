# Rayhunter Client

```
 ____             _                 _
|  _ \ __ _ _   _| |__  _   _ _ __ | |_ ___ _ __
| |_) / _` | | | | '_ \| | | | '_ \| __/ _ \ '__|
|  _ < (_| | |_| | | | | |_| | | | | ||  __/ |
|_| \_\__,_|\__, |_| |_|\__,_|_| |_|\__\___|_|
            |___/
  ____ _ _            _
 / ___| (_) ___ _ __ | |_
| |   | | |/ _ \ '_ \| __|
| |___| | |  __/ | | | |_
 \____|_|_|\___|_| |_|\__|
```

**IMSI catcher detection status display for Flipper Zero via EFF's Rayhunter**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.x-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-WiFi-green)](.)

> **For cellular network security monitoring and privacy defense.**

---

## What It Does

Rayhunter Client provides a real-time threat display for the [EFF Rayhunter](https://github.com/EFF-Rayhunter/rayhunter) IMSI catcher detection system. It connects to a Rayhunter daemon (running on an Orbic RC400L hotspot) via an ESP32 WiFi bridge and displays the current threat level on the Flipper Zero's screen.

IMSI catchers (also known as Stingrays or cell-site simulators) are fake cell towers used by law enforcement and malicious actors to intercept mobile communications. Rayhunter monitors the cellular baseband for suspicious behavior that indicates an IMSI catcher is active.

Threat levels:
- **CLEAN** — no suspicious cellular activity
- **LOW** — minor anomalies detected
- **MEDIUM** — suspicious patterns (e.g., unusual cell tower behavior)
- **HIGH** — strong indicators of IMSI catcher (null cipher, identity requests)

Detected threat indicators include:
- Null cipher negotiation (encryption disabled by tower)
- Identity requests (IMSI/IMEI solicitation)
- Suspicious cell tower changes
- Unusual network behavior patterns

---

## Hardware Requirements

| Component | Details |
|---|---|
| **Flipper Zero** | Running official firmware 1.4.x |
| **ESP32 Dev Board** | Running a WiFi bridge sketch (proxies HTTP to Rayhunter daemon) |
| **Orbic RC400L Hotspot** | Running [Rayhunter daemon](https://github.com/EFF-Rayhunter/rayhunter) |
| **Connection** | Flipper -> ESP32 via UART (115200 baud); ESP32 -> Orbic via WiFi |

### Data Flow

```
Flipper Zero --UART--> ESP32 WiFi Bridge --HTTP--> Orbic RC400L (Rayhunter daemon)
                                                        |
                                          Monitors cellular baseband
                                          for IMSI catcher indicators
```

The ESP32 acts as a bridge: the Flipper sends `rayhunter_poll` commands over UART, the ESP32 makes HTTP GET requests to the Rayhunter API (`/api/analysis-report/live`), and forwards the NDJSON response back to the Flipper.

---

## Installation

**Build:**
```bash
cd rayhunter_client
ufbt
```

**Deploy:**
```
dist/rayhunter_client.fap  ->  /ext/apps/WiFi/rayhunter_client.fap
```

---

## Usage

### Main View

```
Ray Hunter                    [*]
----------------------------------
Status: Recording: active

     !! HIGH THREAT !!

Alert: null cipher
Pkts: 142  Warn: 3
----------------------------------
192.168.1.1:8080
```

- Connection indicator: filled dot = connected, outline = disconnected
- Threat box inverts (white-on-black) for HIGH and MEDIUM threats
- HIGH threat triggers red LED blink + vibration

### Navigation

```
Main View
├── OK / Left  -> Settings (host, port, poll interval)
├── Right      -> About (description and usage info)
└── Back       -> Exit app
```

### Settings

| Setting | Options | Default |
|---|---|---|
| Host | 192.168.1.1 / 192.168.0.1 / 10.0.0.1 / 172.16.0.1 | 192.168.1.1 |
| Port | 8080 / 80 / 8000 / 8888 | 8080 |
| Poll Every | 2s / 5s / 10s / 30s / 60s | 5s |

Settings are applied immediately. The poll timer restarts when the interval is changed.

---

## Architecture

```
rayhunter.c (GUI + poll timer)
     |
     v (poll every N seconds)
     |
rayhunter_worker.c (send poll cmd, parse NDJSON response)
     |
     v (UART command: "rayhunter_poll\n")
     |
rayhunter_uart.c (ISR -> stream buffer -> RX worker -> line callback)
     |
     v (ESP32 WiFi bridge proxies HTTP to Rayhunter daemon)
```

| File | Purpose |
|---|---|
| `rayhunter.c` / `.h` | Main app, 3-view GUI, poll timer, settings, notifications |
| `rayhunter_worker.c` / `.h` | NDJSON keyword parser, threat classification, status extraction |
| `rayhunter_uart.c` / `.h` | UART abstraction with Rayhunter-specific connection detection |

---

## Limitations

- **Requires full hardware chain** — Flipper + ESP32 + Orbic RC400L with Rayhunter installed
- **Keyword-based parsing** — threat classification is heuristic (looks for keywords like "null cipher", "High", "Medium" in NDJSON)
- **Settings not persisted** — configuration resets to defaults on app restart
- **Polling model** — status updates depend on poll interval; not real-time push
- **ESP32 bridge required** — the Flipper cannot make HTTP requests directly

---

## Legal Disclaimer

Rayhunter Client is a **defensive privacy tool** that helps detect unauthorized surveillance equipment. It does not interfere with cellular networks or radio communications. IMSI catcher detection is legal in most jurisdictions as it involves only passive monitoring of your own device's cellular connection.
