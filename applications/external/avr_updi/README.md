# AVR UPDI

Program AVR microcontrollers over **UPDI** from a Flipper Zero. This is not ISP - for classic SPI/ISP (ATmega328P and similar) use the Flipper [AVR ISP Programmer](https://lab.flipper.net/apps/avr_isp) app.

## Features

- Detect chip (signature, SIB, NVM family, sizes)
- Dump flash + EEPROM + fuses to a named folder on the SD card
- Flash an Intel HEX file (chip erase + verify)
- Read / write EEPROM
- Read fuses; write a single fuse (double-confirmed)
- USB **SerialUPDI** bridge for avrdude / pymcuprog (-c serialupdi)
- Pin pair 15/16 or 13/14

## Supported chips

Parts listed in the device table (signature match required for flash/dump/EEPROM). Unknown signatures still show SIB/NVM family on Detect, but programming is refused until they are added.

**P:2 — AVR Dx / DD**

- AVR128DA64 / 48 / 32 / 28 (+ S variants)
- AVR128DB64 / 48 / 32 / 28
- AVR64DA64 / 48 / 32 / 28
- AVR64DB64 / 48 / 32 / 28
- AVR32DA48 / 32 / 28
- AVR64DD32 / 28 / 20 / 14
- AVR32DD32 / 28 / 20 / 14
- AVR16DD32 / 28 / 20 / 14

**P:0 — megaAVR 0 / tinyAVR 1–2**

- ATmega4809 / 4808 / 3209 / 3208 / 1609 / 1608 / 809 / 808
- ATtiny3226 / 3224 / 1626 / 1624 / 1616 / 1614
- ATtiny816 / 814 / 416 / 414 / 412 / 214 / 212

**P:3 — AVR EA**

- AVR64EA48 / 32 / 28
- AVR32EA48 / 32 / 28
- AVR16EA48 / 32 / 28

**P:4 — AVR DU**

- AVR64DU32 / 28
- AVR32DU32 / 28 / 20 / 14
- AVR16DU32 / 28 / 20 / 14

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

MIT - see LICENSE.
