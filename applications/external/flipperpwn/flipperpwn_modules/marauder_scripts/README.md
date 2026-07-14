# Marauder Scripts

Attack chain modules and PC-side serial utilities for WiFi Marauder.

## FlipperPwn Modules (`fpwn/`)

Copy to Flipper SD: `/ext/flipperpwn/modules/credential/`

| Module | Phases | Novel Aspect |
|---|---|---|
| `evil_twin.fpwn` | Scan → Deauth → Portal | Full evil twin in one module |
| `probe_karma_portal.fpwn` | Probe sniff → Identify top SSID → Karma + portal | Targeted karma (not broadcast) |
| `pmkid_harvest.fpwn` | PMKID capture + parallel portal | Dual-vector: hash AND cleartext |
| `wifi_survey_report.fpwn` | Scan → Station scan → Probe sniff → HID report | Auto-generates pentest report via keyboard |
| `ble_chaos.fpwn` | iOS + SwiftPair + Samsung + AirTag in sequence | Full BLE coverage sweep |

## Python Tools (`serial_tools/`)

Requires: `pip install pyserial`

| Tool | Use |
|---|---|
| `marauder_serial.py` | Interactive shell with macros + auto PCAP save |
| `pcap_capture.py` | Dedicated PCAP capture with Wireshark-compatible output |

See [../README.md](../README.md) for full usage documentation.
