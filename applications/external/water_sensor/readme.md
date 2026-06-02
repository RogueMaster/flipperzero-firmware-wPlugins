# Water Sensor Reader for Flipper Zero

**Water Sensor Reader** is a simple Flipper Zero application that reads an analog water sensor connected to a GPIO pin and displays its values in real time. The app shows both the raw ADC value and the corresponding voltage in millivolts, along with a visual progress bar.

## Wiring

Connect the analog output of the water sensor to pin **C0** of the Flipper Zero. The sensor is read on ADC channel 1.

## Features

- Reads analog input from a water sensor on GPIO pin C0.
- Displays **RAW ADC values** (0 to 4095 for the 12-bit ADC).
- Converts raw values to **millivolts** (mV).
- Shows a **progress bar** to visualize the sensor level.
- Updates in real time and exits with the Back button.
- Simple GUI built on the Flipper Zero viewport.

## Usage

1. Connect the water sensor to pin C0 and ground.
2. Launch **Water Sensor reader** from the GPIO apps menu.
3. Read the live RAW value, voltage in mV, and the level bar.
4. Press **Back** to exit.

## Author

Made by Matvey Strelov.
