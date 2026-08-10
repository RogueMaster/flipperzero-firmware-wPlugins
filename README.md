# WiFi Internet Watch for Flipper Zero

Flipper Zero FAP application that uses an official Wi-Fi Developer Board running ESP-AT.

## Features

- Scans nearby Wi-Fi networks
- Lets you select an SSID and enter its password
- Connects through ESP-AT
- Checks internet availability every 60 seconds
- Shows `ONLINE`, `OFFLINE`, or `NO WI-FI`
- Plays a notification when the state changes from offline to online
- Uses Back to exit

## Hardware

- Flipper Zero
- Official Wi-Fi Developer Board based on ESP32-S2
- microSD card in the Flipper Zero

## ESP-AT configuration

The official board UART connected to Flipper uses:

- ESP32-S2 UART TX: GPIO17
- ESP32-S2 UART RX: GPIO18
- Baud rate: 115200
- Hardware flow control: disabled

The application opens `FuriHalSerialIdLpuart`. The factory parameter source used for the
ESP-AT NVS partition is available in `firmware/mfg_nvs_flipper.csv`.

The ESP-AT firmware itself is not redistributed by this repository. Download a compatible
ESP32-S2 ESP-AT release from Espressif and replace its `mfg_nvs` partition with one generated
from the included CSV.

## Build

Install [uFBT](https://github.com/flipperdevices/flipperzero-ufbt), then run:

```sh
ufbt
```

The resulting application is:

```text
dist/wifi_internet_watch.fap
```

Copy it to:

```text
/ext/apps/GPIO/wifi_internet_watch.fap
```

## Usage

1. Install the ESP-AT firmware configured for GPIO17/GPIO18.
2. Mount the Wi-Fi Developer Board on Flipper Zero.
3. Open **Apps → GPIO → WiFi Internet Watch**.
4. Select a network, enter its password, and wait for the status screen.

## License

MIT
