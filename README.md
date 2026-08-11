# WiFi Internet Watch for Flipper Zero

Flipper Zero FAP application that uses an official Wi-Fi Developer Board running ESP-AT.

## Features

- Scans nearby Wi-Fi networks
- Lets you select an SSID and enter its password
- Remembers up to 16 Wi-Fi credentials on the Flipper microSD card
- Lets you update or forget a saved network
- Connects through ESP-AT
- Checks internet availability every 60 seconds
- Shows `ONLINE`, `OFFLINE`, or `NO WI-FI`
- Blinks red while waiting for the internet check
- Plays a notification and retries when Wi-Fi drops
- Plays a notification and blinks green when connectivity returns
- Uses `1.1.1.1` as the default internet check target
- Lets you change the ping IP from the network menu
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

Saved networks are marked with `*`. Press **OK** to connect using the saved password,
hold **OK** to edit it, or press **Right** (`Forget`) to delete it. New and updated
passwords are saved only after ESP-AT successfully joins the network.

Press **Left** in the network list to edit the internet check IP address.

The password screen supports lowercase and uppercase letters, digits, spaces, and common
symbols. Select the `Aa`, `#?`, or `ab` key to switch between lowercase, uppercase, and
symbol layouts. Select `OK` on the keyboard to connect, and use **Back** to delete.

Credentials are stored in the application's private data file on the microSD card. They
are not encrypted, so anyone with access to the card can recover them.

## License

MIT
