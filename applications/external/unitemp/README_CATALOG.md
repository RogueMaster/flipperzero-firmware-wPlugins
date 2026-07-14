
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

| Model                | Temperature range(accuracy, step)| Humidity range(accuracy, step)| Extra range(accuracy, step)| Interface     |
|:--------------------:|:---------------------------:|:--------------------:|:---------------------------:|:-------------:|
|AHT10                 |-40...85°C(±0.3°C, 0.01°C)   |0...100%(±2%, 0.024%) |                             |[I²C](README.md#ic)|
|AHT20                 |-40...85°C(±0.3°C, 0.01°C)   |0...100%(±2%, 0.024%) |                             |[I²C](README.md#ic)|
|AM2320                |-40...80°C(±0.5°C, 0.1°C)    |0...100%(±3%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)/[I²C](README.md#ic)|
|BME280                |-40...85°C(±1.0°C, 0.01°C)   |0...100%(±3%, 0.008%) |300...1100 hPa(±1.0 hPa, 0.0016 hPa) |[I²C](README.md#ic)|
|BME680                |-40...85°C(±0.5°C, 0.01°C)   |0...100%(±3%, 0.008%) |300...1100 hPa(±0.6h Pa, 0.18 hPa)   |[I²C](README.md#ic)|
|BMP180                |-40...85°C(±0.5°C, 0.01°C)   |                      |300...1100 hPa(±1.0 hPa, 0.01 hPa)   |[I²C](README.md#ic)|
|BMP280                |-40...85°C(±1.0°C, 0.01°C)   |                      |300...1100 hPa(±1.0 hPa, 0.0016 hPa) |[I²C](README.md#ic)|
|DHT11 (AOSONG)        | 0...50°C(±2°C, 1.0°C)       | 20...90%(±5%, 1.0%)  |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|
|DHT11 (ASAIR)         | -20...60°C(±2°C, 0.1°C)     | 5...95%(±5%, 1.0%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|
|DHT12                 | -20...60°C(±0.5°C, 0.1°C)   | 20...90%(±5%, 0.1%)  |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|
|DHT20/AM2108          |-40...80°C(±0.5°C, 0.1°C)    |0...100%(±3%, 0.1%)   |                             |[I²C](README.md#ic)|
|DHT21/AM2301          |-40...80°C(±1.0°C, 0.1°C)    |0...100%(±3%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|
|DHT22/AM2302          |-40...80°C(±0.5°C, 0.1°C)    |0...100%(±2%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|
|DS18B20               |-55...125°C(±0.5°C, 0.0625°C)|                      |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|
|DS18S20 (DS1820)      |-55...125°C(±0.5°C, 0.5°C)   |                      |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|
|DS1822                |-55...125°C(±2.0°C, 0.0625°C)|                      |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|
|HDC1080               |-40...125°C(±0.2°C, 0.1°C)   |0...100%(±2%, 0.1%)   |                             |[I²C](README.md#ic)|
|HDC2080               |-40...125°C(±0.2°C, 0.1°C)   |0...100%(±2%, 0.1%)   |                             |[I²C](README.md#ic)|
|HTU21D(F)             |-40...125°C(±0.3°C, 0.1°C)   |0...100%(±2%, 0.04%)  |                             |[I²C](README.md#ic)|
|LM75                  |-55...125°C(±2.0°C, 0.1°C)   |                      |                             |[I²C](README.md#ic)|
|MAX31725              |-40...105°C(±0.5°C, 0.004°C) |                      |                             |[I²C](README.md#ic)|
|MAX31855              |-200...1800°C(±2.0°C, 0.25°C)|                      |                             |[SPI](README.md#1-wire-ds18b20-and-etc)|
|MAX6675               |0...1024°C(±9.0°C, 0.25°C)   |                      |                             |[SPI](README.md#1-wire-ds18b20-and-etc)|
|SCD30                 |0...50°C(±0.4°C, 0.01°C)     |0...100%(±3%, 0.004%) |0...40000 ppm CO₂(±30 ppm, 1.0 ppm) |[I²C](README.md#ic)|
|SCD40                 |-10...60°C(±0.8°C, 0.003°C)  |0...100%(±6%, 0.002%) |400...2000 ppm CO₂(±50 ppm, 1.0 ppm)|[I²C](README.md#ic)|
|SCD41                 |-10...60°C(±0.8°C, 0.003°C)  |0...100%(±6%, 0.00%2) |400...5000 ppm CO₂(±40 ppm, 1.0 ppm)|[I²C](README.md#ic)|
|SHT20                 |-40...125°C(±0.3°C, 0.01°C)  |0...100%(±3%, 0.04%)  |                             |[I²C](README.md#ic)|
|SHT21                 |-40...125°C(±0.3°C, 0.01°C)  |0...100%(±2%, 0.04%)  |                             |[I²C](README.md#ic)|
|SHT25                 |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±1.8%, 0.04%)|                             |[I²C](README.md#ic)|
|SHT30/GXHT30          |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±2%, 0.01%)  |                             |[I²C](README.md#ic)|
|SHT31/GXHT31          |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±2%, 0.01%)  |                             |[I²C](README.md#ic)|
|SHT35/GXHT35          |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±1.5%, 0.01%)|                             |[I²C](README.md#ic)|
|SHT40                 |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|
|SHT41                 |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|
|SHT43                 |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|
|SHT45                 |-40...125°C(±0.1°C, 0.01°C)  |0...100%(±1%, 0.01%)  |                             |[I²C](README.md#ic)|
|SHTC3                 |-40...125°C(±0.2°C, 0.01°C)  |0...100%(±2%, 0.01%)  |                             |[I²C](README.md#ic)|
|Si7021                |-40...125°C(±0.3°C, 0.01°C)  |0...100%(±2%, 0.025%) |                             |[I²C](README.md#ic)|
|TMP102                |-40...125°C(±0.5°C, 0.06°C)  |                      |                             |[I²C](README.md#ic)|

A comprehensive overview of the sensors can be found here (RU): https://kotyara12.ru/iot/th_sensors/
