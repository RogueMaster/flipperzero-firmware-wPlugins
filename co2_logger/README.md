# About

App for reading CO2 ppm from MH-Z19 sensor and saving to a local csv file. 

## 🌐 Web App - Visualize Your Data

**[CO2 Logger Viewer](https://github.com/harryob2/co2_web_app)** — Plug in your Flipper, click connect, and instantly see your CO2 data as an interactive chart. No drivers, no apps, just your browser.

## Credits

This project is based on the original [MH-Z19 UART app](https://github.com/skotopes/flipperzero_mhz19_uart) by [Aku](https://github.com/skotopes). Enhanced with CSV logging functionality for data collection and analysis.

# Pinout

Flipper | Sensor
--------|-------
1       | 5V
8       | GND
15      | RX
16      | TX


# CSV Logging

The app automatically creates a CSV file at /ext/apps_data/co2_logger/co2_log.csv on the SD card and logs valid CO2 readings at the configured interval with the following format:

Timestamp,CO2_PPM
2024-01-15 14:30:25,420
2024-01-15 14:30:55,425

The CSV file includes:
- Timestamp in YYYY-MM-DD HH:MM:SS format
- CO2 concentration in PPM

**Data Quality Features:**
- Configurable logging intervals: 15s, 30s, or 60s (default: 30s)

# Features

- **Auto-Dim Control**: Toggle the automatic screen dimming functionality in settings. When disabled, the screen will stay bright during use.
- **Configurable Logging**: Set logging intervals of 15s, 30s, or 60s to match your monitoring needs.
- **CSV Logging Toggle**: Enable or disable CSV file logging in settings. When enabled, CO2 readings are saved to the SD card.


