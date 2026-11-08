# Third-Party Notices

The CAN Commander firmware source in this repository is licensed under the
PolyForm Noncommercial License 1.0.0 (see `LICENSE.md`). That license covers
only the CAN Commander source code.

The firmware is built against the third-party components below. They are not
included in this repository; they are installed separately (Arduino Library
Manager / Boards Manager) and remain under their own licenses. Any prebuilt
firmware binary contains code from these components, so binary distributions
must include these notices and comply with the listed licenses.

| Component | Used for | License |
|---|---|---|
| [Espressif Arduino core for ESP32](https://github.com/espressif/arduino-esp32) (tested 3.3.4) | Runtime, TWAI, Wi-Fi, NVS, SPI, UART | LGPL-2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) (via the Arduino core) | Low-level drivers | Apache-2.0 |
| [ACAN2517FD](https://github.com/pierremolinaro/acan2517FD) by Pierre Molinaro (tested 2.1.16) | CAN1 backend for MCP2517FD/MCP2518FD | MIT |
| [autowp-mcp2515](https://github.com/autowp/arduino-mcp2515) (tested 1.3.1) | CAN1 backend for MCP2515 | MIT |
| [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) (tested 1.15.4) | Status LEDs | LGPL-3.0 |

## LGPL components

The Arduino core (LGPL-2.1) and Adafruit NeoPixel (LGPL-3.0) are linked into
the firmware binary. The LGPL lets you link them with code under other terms,
provided recipients can obtain the LGPL component source and relink the
firmware against a modified version of it. Publishing the CAN Commander
firmware source together with build instructions satisfies the relinking
requirement. The LGPL components themselves stay under the LGPL; the
PolyForm Noncommercial terms do not apply to them.
