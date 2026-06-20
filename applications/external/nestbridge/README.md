# NestBridge

Control a NoLongerEvil flashed Nest thermostat from a Flipper Zero using BLE advertisements. No phone, no pairing, no connection between devices.

## How It Works

The Flipper runs a custom FAP that broadcasts a BLE advertisement packet when you select a command from the menu. The packet contains a two byte app marker, a command byte, and a temperature byte. An ESP32 development board running a MicroPython script scans for those packets continuously. When it sees one with the right marker it pulls the command out and makes the corresponding API call to the thermostat over WiFi. The two devices never connect to each other.

## Requirements

- Flipper Zero (Momentum firmware tested but any should work)
- ESP32 development board with WiFi and BLE support running MicroPython — tested on [Lonely Binary ESP32-S3](https://a.co/d/0e6yDBgc)
- Nest Gen 1 or Gen 2 thermostat running NoLongerEvil firmware
- NLE API key with read and write scopes
- WiFi network with internet access

## Files

- `nest_bridge.c` — Flipper FAP source, build with ufbt
- `main.py` — MicroPython script for ESP32
- `Nest.fap` — Prebuilt FAP

## Setup

### ESP32

1. Flash MicroPython to your ESP32 board
2. Open `main.py` and fill in the following at the top of the file:
   - `WIFI_SSID` — your WiFi network name
   - `WIFI_PASSWORD` — your WiFi password
   - `NLE_API_KEY` — your NLE API key (starts with `nle_` ~40 characters)
   - `DEVICE_ID` — your NLE device ID (36 characters, format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
   - `SERIAL` — your device serial number (16 characters, found on NLE dashboard)
3. Save `main.py` to the device — it runs automatically on boot
4. Plug into any USB charger

### Flipper

**Option 1 — Use the prebuilt FAP (easiest)**
1. Download `Nest.fap` from this repo
2. Copy it to your Flipper SD card under `apps/`

**Option 2 — Build from source**
1. Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt)
2. Download `nest_bridge.c` from this repo
3. Run `ufbt` in the directory containing the file
4. Copy `dist/nest_bridge.fap` to your Flipper SD card under `apps/`

## Packet Reference

| Command | Value |
|---------|-------|
| Idle | 0x00 |
| Heat | 0x04 |
| Cool | 0x05 |
| Heat-Cool | 0x06 |
| Off | 0x07 |
| Set Temp | 0xF0 (temp in second byte) |
| Fan 15 min | 0xA0 |
| Fan 30 min | 0xA1 |
| Fan 45 min | 0xA2 |
| Fan 1 hour | 0xA3 |
| Fan 2 hours | 0xA4 |
| Fan 4 hours | 0xA5 |
| Fan 8 hours | 0xA6 |
| Fan 12 hours | 0xA7 |
| Fan Off | 0xA8 |

## Usage

1. Open NestBridge on the Flipper
2. Select a command from the menu
3. Press OK
4. Flipper vibrates when the packet is sent

## Notes

- ESP32 must be powered and on WiFi for commands to execute
- Flipper Bluetooth must be enabled
- BLE advertisement range is roughly 30 feet
- The app marker `NB` with manufacturer ID `0x02E5` prevents random BLE devices from triggering commands
