# Moisture Sensor Readings for Flipper Zero

A Flipper Zero application that reads values from a Capacitive Moisture Sensor v1.2.

## Features

- Real-time moisture percentage display (0-100%)
- Visual moisture bar indicator
- Status text (Very Dry, Dry, Moist, Wet, Very Wet)
- Millivolt and raw ADC value display
- Averaged readings for stability
- In-app calibration menu with persistent storage

## Hardware Requirements

- Flipper Zero
- Capacitive Moisture Sensor v1.2

## Wiring

Connect the sensor to the Flipper Zero GPIO header:

| Sensor Pin | Flipper Zero Pin |
|------------|------------------|
| VCC        | Pin 9 (3.3V)     |
| GND        | Pin 18 (GND)     |
| AOUT       | Pin 16 (PC3)     |

## Usage

1. Connect the moisture sensor to your Flipper Zero
2. Navigate to Apps > GPIO > Moisture Sensor
3. The display shows:
   - Moisture percentage
   - Status indicator
   - Millivolt reading and raw ADC value
4. Press Left to open calibration menu
5. Press Back to exit

## Calibration

Press Left to open the calibration menu:

- Dry / Wet - Adjust ADC values
- Reset - Restore factory defaults
- Save - Save calibration values

Calibration values persist across restarts.
