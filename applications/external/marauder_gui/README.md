# Marauder GUI

Marauder GUI is a bilingual Flipper Zero companion application for controlling an ESP32 Marauder
board over GPIO UART. It provides structured menus for the Marauder serial CLI, so commands do not
need to be typed manually.

## Hardware and connection

- Flipper Zero
- An ESP32 board running compatible ESP32 Marauder firmware
- Flipper GPIO 13 (TX) to Marauder RX
- Flipper GPIO 14 (RX) to Marauder TX
- Common GND
- 115200 baud, 3.3 V UART logic

The application does not include or modify ESP32 Marauder firmware. Wi-Fi and Bluetooth operations
are performed by the connected ESP32 board.

## Features

- Wi-Fi scanning and AP selection
- Wi-Fi testing and beacon tools
- Bluetooth testing and tracker tools
- Wi-Fi and Bluetooth detectors
- Ping, ARP, and port scanning
- Device information and Marauder settings
- Raw serial terminal
- Persistent Turkish and English interface

## Installation

Copy marauder_gui.fap to apps/GPIO/ on the Flipper Zero SD card, or install the application from
the Flipper Apps Catalog when available.

## Responsible use

Use wireless testing features only on systems you own or have explicit permission to test. Users
are responsible for complying with applicable laws and regulations.

## Author

mel4mi
