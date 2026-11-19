# AVR UPDI

Program AVR microcontrollers over **UPDI** from a Flipper Zero. This is not ISP — for classic SPI/ISP (ATmega328P and similar) use the Flipper [AVR ISP Programmer](https://lab.flipper.net/apps/avr_isp) app.

## Features

- Detect chip (signature, SIB, NVM family, sizes)
- Dump flash + EEPROM + fuses to a named folder on the SD card
- Flash an Intel HEX file (chip erase + verify)
- Read / write EEPROM
- Read fuses; write a single fuse (double-confirmed)
- USB **SerialUPDI** bridge for avrdude / pymcuprog (-c serialupdi)
- Pin pair 15/16 or 13/14

Supported UPDI NVM families: **P:0** (tinyAVR / megaAVR 0), **P:2** (AVR Dx), **P:3** (AVR EA), **P:4** (AVR DU). Unknown signatures are identified by SIB but refuse flash/dump until listed in the device table.

## Wiring

**Bare Flipper GPIO (3.3 V only)**

| Flipper | Target |
| --- | --- |
| 15 TX through ~1 kΩ (or 13 TX) | UPDI |
| 16 RX direct (or 14 RX) | same UPDI node |
| GND | GND |
| 9 3V3, optional | VCC |

Do not connect a 5 V target directly to Flipper GPIO. Do not put 12 V on a GPIO pin (no HV-UPDI).

**Companion level-shifter board**

For 5 V targets and combined ISP/UPDI/UART breakout, use [gztproject/AVR_programmer_debugger](https://github.com/gztproject/AVR_programmer_debugger). That KiCad Flipper accessory includes level shifters for AVR ISP, UPDI, and UART debugging.

Close the USB-UART bridge before opening the app. The app takes the selected UART and turns expansion-module detection off while it runs.

## Dump layout

Dumps are written under /ext/apps_data/avr_updi/name/:

- flash.hex
- eeprom.hex
- fuses.txt

## SerialUPDI (USB)

**SerialUPDI (USB)** exposes a second USB CDC port. Wire UPDI as above, then on the host run avrdude with -c serialupdi on the new COM/tty port. Exact port name depends on your OS. Baud defaults to 115200 8E2.

## Build

Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt), then from this directory run ufbt. Copy dist/avr_updi.fap to apps/GPIO/ on the SD card.

Host HEX tests: compile tests/test_intel_hex.c with hex/intel_hex.c and run the binary.

## License

MIT — see LICENSE.
