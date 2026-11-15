# DrMosfetCan

Use Flipper Zero as a USB-CAN adapter for SavvyCAN and other GVRET clients.

- One CAN 2.0 bus, with standard and extended data frames up to 8 bytes.
- Select 50, 125, 250, 500 or 1000 kbit/s from the host; defaults to 500 kbit/s.
- View TX/RX counters, the last frame, CAN/USB status and an error counter.
- Press Back to stop the bridge and restore the previous USB configuration.

## Hardware

Requires an MCP2515 module with a 16 MHz oscillator, a CAN transceiver and 3.3 V compatible SPI signals. Connect CS to PA4, SCK to PB3, MOSI to PA7, MISO to PA6, and a shared ground. Power the module according to its specification; the app does not enable the 5 V output.

## Connecting

Start the app, then select GVRET and the second Flipper USB serial port in SavvyCAN. The first serial port remains available for the CLI. USB OK indicates that the host has opened the bridge port.

CAN FD and RTR frames are not supported. The app does not program ECUs or write files to the SD card.
