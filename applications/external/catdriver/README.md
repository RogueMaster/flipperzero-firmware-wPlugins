# CatDriver

Advanced Wi-Fi and Bluetooth Low Energy wardriving application for Flipper Zero paired with an ESP32-S3 and GPS module.

## Features
- **ALL Mode:** Simultaneous Wi-Fi 2.4 GHz and BLE wardriving.
- **WIFI Mode:** Dedicated 2.4 GHz 802.11 b/g/n network scanning.
- **BLE Mode:** Bluetooth Low Energy device discovery with RSSI.
- **WiGLE CSV 1.4:** Generates WiGLE-compatible CSV log files on the microSD card in `/ext/apps_data/catwardrive/`.
- **GPS Tagging:** Real-time NMEA GPS location tracking via UART on pins 15 and 16.
- **Session Isolation:** Creates distinct timestamped CSV logs for each scanning session.
- **Power Saving:** ESP32 remains in idle state until scanning starts.
- **Multi-language:** English and Spanish user interface.

## Hardware Requirements
- Flipper Zero
- ESP32-S3 module connected to USART pins 13 (TX) and 14 (RX)
- NMEA GPS module (such as NEO-6M or ATGM336H) connected to LPUART pins 15 (TX) and 16 (RX)

## Pin Configuration
- ESP32-S3 module connected to USART pins 13 (TX) and 14 (RX) at 115200 baud.
- NMEA GPS module connected to LPUART pins 15 (TX) and 16 (RX) at 9600 baud.
- Communicates concurrently through both hardware serial interfaces on the Flipper Zero expansion port.

## ESP32 Firmware
Firmware source and pre-compiled binaries are provided in the GitHub repository releases.
