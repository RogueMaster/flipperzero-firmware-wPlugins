# INA Meter for Flipper Zero

INA Meter is an application for Flipper Zero that allows you to read I2C-connected current/power monitors from Texas Instruments.

## Supported Sensors:

- **INA219** – 0–26V, 16-bit resolution (±0.5% accuracy)
- **INA226** – 0–36V, 16-bit resolution (±0.1% accuracy)
- **INA228** – 0–85V, 20-bit resolution (±0.05% accuracy)

## Wiring

| Flipper pin  | INAxxx |
| ------------ | ------ |
| C0 (16)      | SCL    |
| C1 (15)      | SDA    |

## Instructions for Use:

- Launch the application and go to the configuration menu by pressing the LEFT button.
- Select the sensor type, configure its I2C address, and set the shunt resistance.
- Once the settings are complete, press the BACK button to return to the Current Gauge Screen.
- If the sensor is connected properly, the Current Gauge Screen will display the bus voltage and the current flowing through the shunt resistor.

The application allows measured values to be saved to a CSV file on the SD card. A CSV file is automatically created when recording starts and is stored in the */apps_data/ina_meter/logs* folder. Its name is derived from the date and time the recording begins.

You can quickly switch between two shunt resistors without manually changing the value each time, by long-pressing the down button on the gauge screen.

## Precision and Sampling Presets

The application provides a simple way to configure conversion time and sample averaging with four preset levels: Low, Medium, High, and Max. These settings are sensor-specific:

|              | Low       | Medium   | High     |  Max   |
| ------------ | --------- | -------- | -------- |--------|
| INA219       | 532μs x 128    | 532μs x 128    | 532μs x 128  | 532us x 128  |
| INA226       | 140μs x 1024    | 588μs x 1024  | 2116μs x 1024  | 8244us x 1024 |
| INA228       | 50μs x 1024    | 280μs x 1024  | 1052μs x 1024  | 4120us x 1024 |

The precision can be set independently for VBUS and shunt voltage measurements.

For INA226 and INA228 devices, the number of averaged samples can be reduced from 1024 down to 256 by changing the Averaging setting from Max to Medium.

## Measurement Range and Resolution

Each sensor operates within a fixed voltage range, with a defined resolution for both bus voltage (VBUS) and shunt voltage (VSHUNT) measurements. The values below indicate the maximum measurable voltage and the smallest detectable voltage step for each parameter.

|              | VBUS Max  | VBUS Resolution | VSHUNT Max | VSHUNT Resolution |
| ------------ | --------- | --------- | --------- | --------- |
| INA219       | 26V       | 4mV       | ±320mV    | 10μV      |
| INA226       | 36V       | 1.25mV    | ±81.92mV  | 2.5μV     |
| INA228       | 85V       | 195.315μV | ±160mV    | 312.5nV   |










