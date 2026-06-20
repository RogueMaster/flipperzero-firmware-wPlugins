# Changelog

## 1.0

Initial release.

- CO2 monitoring via MH-Z19B/C (PWM and UART)
- Climate monitoring via BME280 and other I2C/1-Wire/SPI sensors (from unitemp)
- LED color indicator: green / yellow / orange / red based on CO2 level
- Sound alerts with configurable threshold, hysteresis and cooldown
- PWM signal processing: trimmed mean averaging (1–30 points)
- Configurable PWM range (2000–10000 ppm)
- Backlight control: 5s / Auto / 1m / 5m / 10m / 20m / 60m / Infinity
- Clock and battery display on main screen
- Debug mode with raw PWM data overlay
- Freeze indicator for stale CO2 readings
- Eject button to reset PWM sensor data
- Hot-plug support for sensors
- Wiring info screen for each sensor type
- Settings saved to SD card
- Tested on Official 1.4.2, Unleashed unlshd-086, Momentum mntm-012
