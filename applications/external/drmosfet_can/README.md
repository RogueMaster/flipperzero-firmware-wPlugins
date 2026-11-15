# DrMosfetCan

USB-CAN bridge for Flipper Zero, by Dr Mosfet. Uses an MCP2515 and the GVRET protocol to connect to SavvyCAN.

## Hardware

Use an MCP2515 module with a **16 MHz oscillator**, a CAN transceiver and SPI signals compatible with **3.3 V**.

| Signal | Flipper pin |
| --- | --- |
| CS | PA4 |
| SCK | PB3 |
| MOSI | PA7 |
| MISO | PA6 |
| GND | GND |

Power the module according to its specification. The app does not enable the 5 V output.

## Usage

1. Open **Apps > GPIO > DrMosfetCan**.
2. In SavvyCAN, select a **GVRET** connection and the **second Flipper USB serial port**. The first port remains the CLI.
3. Select the bitrate in the host application. The default is 500 kbit/s; supported rates are 50, 125, 250, 500 and 1000 kbit/s.
4. Press Back to stop the bridge and restore the previous USB configuration.

The display shows TX/RX counters, the last frame, USB/CAN status and errors. CAN 2.0 standard and extended data frames up to 8 bytes are supported. CAN FD and RTR frames are not supported.

## Build

Build against the SDK matching your firmware with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```text
ufbt
```

Copy the resulting FAP to `/ext/apps/GPIO/`. The binary in `dist/` targets API 87.1; the sources also build with the official release SDK 88.2.

## Validation

The footer and launcher icon are checked on a physical Flipper Zero. USB serial enumeration and GVRET handshake are checked separately from physical CAN traffic. See `validation/STATUS.md` for the scope of verification.

Parser tests run the compiled Cortex-M4 code in Unicorn:

```text
python -m pip install -r tests/requirements.txt
python tests/run_tests.py
```

Set `MOMENTUM_ROOT` to a compatible local firmware/toolchain checkout before running these developer tests.

## License and credits

MIT. The MCP2515/SPI driver is derived from Electronic Cats' flipper-MCP2515-CANBUS. See LICENSE and THIRD_PARTY_NOTICES.md.
