# Unitemp changelog
## Unitemp 2.0
##### Unitemp 2.0.1-dev
- Fixed a bug where scanning the OneWire bus was not possible if there was an active sensor on it.
- Added SCD40/41 sensors.
#### New Features and Sensors
- Added LED, sound, and vibration indication of the environment's status. The RGB LED will show the safety level of the thermal index and CO2 concentration in the air. Green means safe, yellow means remove active loads and ventilate the room. Red means the environment is hazardous to health. Flashing red and audible and vibration alarms indicate a fatal health hazard.
- Added scanning for devices connected to the I2C bus when creating or editing a sensor. This will make it easy to find the required address and prevent creating sensors with duplicate addresses.
- Added the ability to connect Dallas sensors (DS18B20, etc.) to port 17 (1W).
- Added the ability to display the thermal index along with the CO2 reading.
- When the application is first launched, units of measurement are automatically selected based on the system's localization settings.
- Added units of measurement when displaying dew point temperature. 
- Added 5V power management setting.
- Added support for the HDC2080 sensor.
#### General
- Sensor configuration and save files have been moved to apps_data. Saved sensors from the previous version of the app will automatically migrate to the new location.
- Fixed temperature index calculation. It is now only calculated for temperatures above 21°C (70°F).
- Temporarily removed SCD40/41 calibration
- Updated the list of sensors in the README.
#### User Interface
- Improved display of CO2 concentration values ​​in the air.
- The backlight is always on only when sensor values ​​are displayed. In the menu, the backlight will be turned off according to the system settings.
- Added a wait state for sensor values. If a sensor is initialized but its values ​​are not yet ready, the message "Reading values..." is displayed. - The "No sensors found" message has been replaced with "No sensors yet."
- The crying dolphin icon has been replaced with a confused one (whatever that means).
- New windows and other GUI improvements.
#### Application Code
- General code quality improvements. The GUI has been completely rewritten using native libraries.
- Memory leaks have been fixed.
- Sensor polling and screen rendering are now performed asynchronously in separate threads.
- Configuration is now saved only upon initial creation or subsequent modification.
- The 1-wire driver has been rewritten using native Flipper Zero firmware functions. Numerous fixes and improvements have been made.
- A check for the initialization of all sensors has been added, along with reinitialization on error.
- The SCD30 driver has been completely rewritten. Sensor values ​​and status are now displayed correctly.

## Unitemp 1.8
- Fixes and improvements. Many thanks to [xMasterX](https://github.com/xMasterX) for their help and support.
## Unitemp 1.7
- Add MAX31725 temperature sensor. 
- Fix bug in SHT3x detection and reading. 
- Decrease SCD40 pooling interval. 
## Unitemp 1.6
- Add SCD30 and SCD40 Co2 sensors. 
## Unitemp 1.5
- Add Cry_dolph_55x52 image (build on latest API) (Thanks jamisonderek)
## Unitemp 1.4
- Remove version value from application manifest
- Small fix (Thanks for JamesDavid)
## Unitemp 1.3
- New FZ API satisfaction
## Unitemp 1.2
- New sensor - BME680 (temperature, humidity, pressure)
- New sensor - MAX31855 (temperature on termocouple)
- New sensor - MAX6675 (temperature on termocouple)
- Added the ability to quickly change the temperature unit by holding the OK key
- Fixes and improvements
## Unitemp 1.1
- Added new sensors: DHT20, AM2108, AHT10, AHT20, GXHT30/31/35, SHT30/31/35, BMP180, HTU21x, HDC1080.
- Fixed incorrect display of negative values of DHT sensors.
- Fixed incorrect editing and display of the I2C address
## Unitemp 1.0
- Application release
