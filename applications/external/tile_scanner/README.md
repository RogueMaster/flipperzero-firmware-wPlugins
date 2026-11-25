# Tile Scanner

Flipper app that uses an external ESP32 to scan for [Tile tags](https://www.life360.com/tile-trackers).

## Flipper

This is developed and tested against [Momentum Firmware](https://github.com/next-flip/momentum-firmware) version [mntm-0012](https://github.com/Next-Flip/Momentum-Firmware/tree/mntm-012).

The Flipper receives MAC address and RSSI values from the ESP32, and displays the data on the screen.
Unknown MAC addresses cause the Flipper to buzz and play a sound to indicate that one is nearby.
Known MAC addresses are read from `/ext/apps_data/tile_scanner/known_macs.txt` with each MAC address being on a separate line and formatted as `XX:XX:XX:XX:XX:XX` (Note: data after the address is ignored).

There are two implementations of this app; [Javascript](./js/), and [Flipper App (FAP)](./src/).
The [Javascript](./js/) is meant as a quick implementation to test the different components with limited functionality.
The [Flipper App (FAP)](./src/) is meant as the real implementation with the full functionality.

## ESP 32

The source for the ESP32 code is found in the [esp32](./esp32/) folder.

It is built using [Platform IO](https://platformio.org/), but is small enough that it could be ported to work with [Arduino IDE](https://docs.arduino.cc/software/ide/).
The [NimBLE](https://github.com/h2zero/NimBLE-Arduino) library is used for the bluetooth scanning.

This code is developed and tested using the [Seeed Studio XIAO ESP32C6](https://www.amazon.com/dp/B0DRNW9LJM) as the ESP32.
This should work with other ESP32s, with only needing to change the UART RX and TX ports.

### Pin Out

| Seeed Studio XIAO ESP32C6 | Flipper     |
| ------------------------- | ----------- |
| TX (D6)                   | RX (Pin 14) |
| RX (D7)                   | TX (Pin 13) |
| 5V                        | 5V (Pin  1) |
| GND                       | GND (Pin 8) |
