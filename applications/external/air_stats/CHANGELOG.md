# Changelog

## Unreleased

- Add SCD40/41 to the CO2 selector, saved settings, and sensor info screen.
- Verify SCD41 periodic CO2, temperature, and humidity readings on Momentum mntm-009.
- Check SCD4x data readiness and propagate I2C/CRC failures without partially updating readings.
- Display SCD4x temperature/humidity when the separate climate sensor has no valid data.
- Correct I2C wiring documentation and info screen: C1/15 is SDA, C0/16 is SCL.
- Support mutable and const I2C SDK handles and remove machine-specific uFBT build paths.
- Add host driver checks and an investigation/validation report.

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
