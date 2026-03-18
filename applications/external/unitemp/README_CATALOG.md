![Flipper usage](https://user-images.githubusercontent.com/10090793/211182642-e41919c5-3091-4125-815a-2d6a77a859f6.png)
# 🌡️ Unitemp - Universal temperature sensor reader
An app for [Flipper Zero](https://flipper.net/products/flipper-zero) that turns your gadget into a multifunctional environmental sensor. It can read data from various sensors you connect to Flipper Zero, for example, temperature, humidity, atmospheric pressure, and even CO₂ levels. You can assess the climate at home or in the office, or simply use Flipper Zero as a portable thermometer.

# Features
- Real-time display of temperature, humidity, pressure, and CO₂ concentration.
- [Heat index](https://en.wikipedia.org/wiki/Heat_index) and [dew point](https://en.wikipedia.org/wiki/Dew_point) temperature display.
- Environmental quality analysis and visual and audible indicators (good 🟢, normal 🟡, poor 🟠, dangerous 🔴)
- Automatic and manual selection of temperature (degrees Celsius/Fahrenheit) and pressure (mmHg/inHg/kPa/hPa) units.
- Support for a [wide range of digital sensors](README.md#list-of-supported-sensors) with [I²C](README.md#ic), [SPI](README.md#1-wire-ds18b20-and-etc), [1-Wire](README.md#1-wire-ds18b20-and-etc), and [Single Wire](README.md#single-wire-dht11-and-etc) connectivity.
- User-friendly and intuitive interface.


# Connecting sensors
To connect, you will need a supported sensor and [Dupont male-female](https://www.aliexpress.com/w/wholesale-Dupont-male-female.html) wires. The connection method depends on the interface.

## Single Wire (DHT11 and etc)
|Sensor pin   | Flipper Zero pin    |
|:-----------:|:-------------------:|
|VCC          |9 (3V3)              |
|GND          |any GND (8,11 or 18) |
|Data         |any free digital port|

## 1-Wire (DS18B20 and etc)
|Sensor pin   | Flipper Zero pin    |
|:-----------:|:-------------------:|
|VCC          |9 (3V3)              |
|GND          |any GND (8,11 or 18) |
|Data         |any free digital port|

Pin 17 (1W) is preferred. You can also connect multiple sensors in parallel using the same circuit.

## SPI (MAX6675, MAX31725 and etc)
|Sensor pin   | Flipper Zero pin    |
|:-----------:|:-------------------:|
|VCC          |9 (3V3)              |
|GND          |any GND (8,11 or 18) |
|MOSI (if any)|2 (A7)               |
|MISO (DO/SO) |3 (A6)               |
|SCK (CLK)    |5 (B3)               |
|CS (SS)      |any free digital port|

## I²C
|Sensor pin| Flipper Zero pin   |
|:--------:|:------------------:|
|VCC       |9 (3V3)             |
|GND       |any GND (8,11 or 18)|
|SDA       |15 (C1)             |
|SCL       |16 (C2)             |

# Need help? Discussions?

Join the discussion, ask a question, or just send a photo of the flipper with sensors to [Discord](https://discord.com/channels/740930220399525928/1056727938747351060). [Invite link](https://discord.com/invite/flipper)

# Contributing 
You can write a driver for your favorite sensor and submit it in pull requests. This is encouraged.

# Gratitudes
- Special thanks [xMasterX](https://github.com/xMasterX), [vladin79](https://github.com/vladin79), [divinebird](https://github.com/divinebird), [jamisonderek](https://github.com/jamisonderek), [kaklik](https://github.com/kaklik)
- [Svaarich](https://github.com/Svaarich) for the UI design 
- Unleashed firmware community for sensors testing and feedbacks
- ...and everyone who helped with development and testing


# List of supported sensors

| Model                | Temperature range<br>(accuracy, step)| Humidity range<br>(accuracy, step)| Extra range<br>(accuracy, step)| Interface     |Image|
|:--------------------:|:-------------------------------:|:------------------------:|:---------------------------:|:-------------:|:--------------:|
|AHT10                 |-40...85°C<br>(±0.3°C, 0.01°C)   |0...100%<br>(±2%, 0.024%) |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/AHT10/general.png)
|AHT20                 |-40...85°C<br>(±0.3°C, 0.01°C)   |0...100%<br>(±2%, 0.024%) |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/AHT20/general.png)
|AM2320                |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)/[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/AM2320/general.png)
|BME280                |-40...85°C<br>(±1.0°C, 0.01°C)   |0...100%<br>(±3%, 0.008%) |300...1100 hPa<br>(±1.0 hPa, 0.0016 hPa) |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/BME280/general.png)
|BME680                |-40...85°C<br>(±0.5°C, 0.01°C)   |0...100%<br>(±3%, 0.008%) |300...1100 hPa<br>(±0.6h Pa, 0.18 hPa)   |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/BME680/general.png)
|BMP180                |-40...85°C<br>(±0.5°C, 0.01°C)   |                          |300...1100 hPa<br>(±1.0 hPa, 0.01 hPa)   |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/BMP180/general.png)
|BMP280                |-40...85°C<br>(±1.0°C, 0.01°C)   |                          |300...1100 hPa<br>(±1.0 hPa, 0.0016 hPa) |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/BMP280/general.png)
|DHT11 (AOSONG)        | 0...50°C<br>(±2°C, 1.0°C)       | 20...90%<br>(±5%, 1.0%)  |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|![Flipper usage](.github/images/sensors/DHT11/DHT11-AOSONG.png)
|DHT11 (ASAIR)         | -20...60°C<br>(±2°C, 0.1°C)     | 5...95%<br>(±5%, 1.0%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|![Flipper usage](.github/images/sensors/DHT11/DHT11-ASAIR.png)
|DHT12                 | -20...60°C<br>(±0.5°C, 0.1°C)   | 20...90%<br>(±5%, 0.1%)  |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|![Flipper usage](.github/images/sensors/DHT12/general.png)
|DHT20/AM2108          |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/DHT20/general.png)
|DHT21/AM2301          |-40...80°C<br>(±1.0°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|![Flipper usage](.github/images/sensors/DHT21/general.png)
|DHT22/AM2302          |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±2%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|![Flipper usage](.github/images/sensors/DHT22/general.png)
|DS18B20               |-55...125°C<br>(±0.5°C, 0.0625°C)|                          |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|![Flipper usage](.github/images/sensors/Dallas/DS18B20.png)
|DS18S20 (DS1820)      |-55...125°C<br>(±0.5°C, 0.5°C)   |                          |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|![Flipper usage](.github/images/sensors/Dallas/TO-92.png)
|DS1822                |-55...125°C<br>(±2.0°C, 0.0625°C)|                          |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|![Flipper usage](.github/images/sensors/Dallas/TO-92.png)
|HDC1080               |-40...125°C<br>(±0.2°C, 0.1°C)   |0...100%<br>(±2%, 0.1%)   |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/HDC1080/general.png)
|HDC2080               |-40...125°C<br>(±0.2°C, 0.1°C)   |0...100%<br>(±2%, 0.1%)   |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/HDC2080/general.png)
|HTU21D(F)             |-40...125°C<br>(±0.3°C, 0.1°C)   |0...100%<br>(±2%, 0.04%)  |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/HTU21/general.png)
|LM75                  |-55...125°C<br>(±2.0°C, 0.1°C)   |                          |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/LM75/general.png)
|MAX31725              |-40...105°C<br>(±0.5°C, 0.004°C) |                          |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/MAX31725/general.png)
|MAX31855              |-200...1800°C<br>(±2.0°C, 0.25°C)|                          |                             |[SPI](README.md#1-wire-ds18b20-and-etc)|![Flipper usage](.github/images/sensors/MAX31855/general.png)
|MAX6675               |0...1024°C<br>(±9.0°C, 0.25°C)   |                          |                             |[SPI](README.md#1-wire-ds18b20-and-etc)|![Flipper usage](.github/images/sensors/MAX6675/general.png)
|SCD30                 |0...50°C<br>(±0.4°C, 0.01°C)     |0...100%<br>(±3%, 0.004%) |0...40000 ppm CO₂<br>(±30 ppm, 1.0 ppm) |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SCD30/general.png)
|SCD40                 |-10...60°C<br>(±0.8°C, 0.003°C)  |0...100%<br>(±6%, 0.002%) |400...2000 ppm CO₂<br>(±50 ppm, 1.0 ppm)|[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SCD4x/SCD40.png)
|SCD41                 |-10...60°C<br>(±0.8°C, 0.003°C)  |0...100%<br>(±6%, 0.00%2) |400...5000 ppm CO₂<br>(±40 ppm, 1.0 ppm)|[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SCD4x/SCD41.png)
|SHT20                 |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±3%, 0.04%)  |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT2x/SHT20.png)
|SHT21                 |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±2%, 0.04%)  |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT2x/SHT21.png)
|SHT25                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.04%)|                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT2x/SHT25.png)
|SHT30/GXHT30          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%)  |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT3x/SHT30.png)
|SHT31/GXHT31          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%)  |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT3x/SHT31.png)
|SHT35/GXHT35          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.5%, 0.01%)|                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT3x/SHT35.png)
|SHT40                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT4x/SHT40.png)
|SHT41                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT4x/SHT41.png)
|SHT43                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT4x/SHT43.png)
|SHT45                 |-40...125°C<br>(±0.1°C, 0.01°C)  |0...100%<br>(±1%, 0.01%)  |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/SHT4x/SHT45.png)
|Si7021                |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±2%, 0.025%) |                             |[I²C](README.md#ic)|![Flipper usage](.github/images/sensors/Si7021/general.png)

A comprehensive overview of the sensors can be found here (RU): https://kotyara12.ru/iot/th_sensors/
