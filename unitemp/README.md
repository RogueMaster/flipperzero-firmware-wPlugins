![Flipper usage](https://user-images.githubusercontent.com/10090793/211182642-e41919c5-3091-4125-815a-2d6a77a859f6.png)
# 🌡️ Unitemp - Universal temperature sensor reader
[![GitHub release](https://img.shields.io/github/release/quen0n/unitemp-flipperzero?include_prereleases=&sort=semver&color=blue)](https://github.com/quen0n/unitemp-flipperzero/releases/)
[![License](https://img.shields.io/github/license/quen0n/unitemp-flipperzero)](https://github.com/quen0n/unitemp-flipperzero/blob/release/LICENSE.md)
[![Build release](https://github.com/quen0n/unitemp-flipperzero/actions/workflows/build_release.yml/badge.svg?branch=master)](https://github.com/quen0n/unitemp-flipperzero/actions/workflows/build_release.yml)

An app for [Flipper Zero](https://flipper.net/products/flipper-zero) that turns your gadget into a multifunctional environmental sensor. It can read data from various sensors you connect to Flipper Zero, for example, temperature, humidity, atmospheric pressure, and even CO₂ levels. You can assess the climate at home or in the office, or simply use Flipper Zero as a portable thermometer.

## Features
- Real-time display of temperature, humidity, pressure, and CO₂ concentration.
- [Heat index](https://en.wikipedia.org/wiki/Heat_index) and [dew point](https://en.wikipedia.org/wiki/Dew_point) temperature display.
- Environmental quality analysis and visual and audible indicators (good 🟢, normal 🟡, poor 🟠, dangerous 🔴)
- Automatic and manual selection of temperature (degrees Celsius/Fahrenheit) and pressure (mmHg/inHg/kPa/hPa) units.
- Support for a [wide range of digital sensors](README.md#list-of-supported-sensors) with [I²C](README.md#ic), [SPI](README.md#1-wire-ds18b20-and-etc), [1-Wire](README.md#1-wire-ds18b20-and-etc), and [Single Wire](README.md#single-wire-dht11-and-etc) connectivity.
- User-friendly and intuitive interface.

## Installation
Just install from the application catalog https://lab.flipper.net/apps/unitemp or the mobile application

..or use the [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) to compile and install directly to Flipper.

    git clone https://github.com/quen0n/unitemp-flipperzero.git
    cd unitemp-flipperzero
    git checkout -b master
    ufbt update
    ufbt && ufbt launch 

## Connecting sensors
To connect, you will need a supported sensor and [Dupont male-female](https://www.aliexpress.com/w/wholesale-Dupont-male-female.html) wires. The connection method depends on the interface.

### Single Wire (DHT11 and etc)
|Sensor pin   | Flipper Zero pin    |
|:-----------:|:-------------------:|
|VCC          |9 (3V3)              |
|GND          |any GND (8,11 or 18) |
|Data         |any free digital port|

### 1-Wire (DS18B20 and etc)
|Sensor pin   | Flipper Zero pin    |
|:-----------:|:-------------------:|
|VCC          |9 (3V3)              |
|GND          |any GND (8,11 or 18) |
|Data         |any free digital port|

Pin 17 (1W) is preferred. You can also connect multiple sensors in parallel using the same circuit.

### SPI (MAX6675, MAX31725 and etc)
|Sensor pin   | Flipper Zero pin    |
|:-----------:|:-------------------:|
|VCC          |9 (3V3)              |
|GND          |any GND (8,11 or 18) |
|MOSI (if any)|2 (A7)               |
|MISO (DO/SO) |3 (A6)               |
|SCK (CLK)    |5 (B3)               |
|CS (SS)      |any free digital port|

### I²C
|Sensor pin| Flipper Zero pin   |
|:--------:|:------------------:|
|VCC       |9 (3V3)             |
|GND       |any GND (8,11 or 18)|
|SDA       |15 (C1)             |
|SCL       |16 (C2)             |

## Need help? Discussions?

Join the discussion, ask a question, or just send a photo of the flipper with sensors to [Discord](https://discord.com/channels/740930220399525928/1056727938747351060). [Invite link](https://discord.com/invite/flipper)

## Contributing 
You can write a driver for your favorite sensor and submit it in pull requests. This is encouraged.

## Gratitudes
- Special thanks [xMasterX](https://github.com/xMasterX), [vladin79](https://github.com/vladin79), [divinebird](https://github.com/divinebird), [jamisonderek](https://github.com/jamisonderek), [kaklik](https://github.com/kaklik)
- [Svaarich](https://github.com/Svaarich) for the UI design 
- [Unleashed firmware](https://github.com/DarkFlippers/unleashed-firmware) community for sensors testing and feedbacks
- ...and everyone who helped with development and testing


## List of supported sensors

| Model                | Temperature range<br>(accuracy, step)| Humidity range<br>(accuracy, step)| Extra range<br>(accuracy, step)| Interface     |Image and link |
|:--------------------:|:-------------------------------:|:------------------------:|:---------------------------:|:-------------:|:--------------:|
|AHT10                 |-40...85°C<br>(±0.3°C, 0.01°C)   |0...100%<br>(±2%, 0.024%) |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\AHT10\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-AHT10-module.html)|
|AHT20                 |-40...85°C<br>(±0.3°C, 0.01°C)   |0...100%<br>(±2%, 0.024%) |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\AHT20\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-AHT20-module.html)|
|AM2320                |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)/[I²C](README.md#ic)|[<img src=".github\images\sensors\AM2320\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-AM2320-module.html)|
|BME280                |-40...85°C<br>(±1.0°C, 0.01°C)   |0...100%<br>(±3%, 0.008%) |300...1100 hPa<br>(±1.0 hPa, 0.0016 hPa) |[I²C](README.md#ic)|[<img src=".github\images\sensors\BME280\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-BME280-module.html)|
|BME680                |-40...85°C<br>(±0.5°C, 0.01°C)   |0...100%<br>(±3%, 0.008%) |300...1100 hPa<br>(±0.6h Pa, 0.18 hPa)   |[I²C](README.md#ic)|[<img src=".github\images\sensors\BME680\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-BME680-module.html)|
|BMP180                |-40...85°C<br>(±0.5°C, 0.01°C)   |                          |300...1100 hPa<br>(±1.0 hPa, 0.01 hPa)   |[I²C](README.md#ic)|[<img src=".github\images\sensors\BMP180\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-BMP180-module.html)|
|BMP280                |-40...85°C<br>(±1.0°C, 0.01°C)   |                          |300...1100 hPa<br>(±1.0 hPa, 0.0016 hPa) |[I²C](README.md#ic)|[<img src=".github\images\sensors\BMP280\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-BMP280-module.html)|
|DHT11 (AOSONG)        | 0...50°C<br>(±2°C, 1.0°C)       | 20...90%<br>(±5%, 1.0%)  |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|[<img src=".github\images\sensors\DHT11\DHT11-AOSONG.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DHT11-module.html)|
|DHT11 (ASAIR)         | -20...60°C<br>(±2°C, 0.1°C)     | 5...95%<br>(±5%, 1.0%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|[<img src=".github\images\sensors\DHT11\DHT11-ASAIR.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DHT11-module.html)|
|DHT12                 | -20...60°C<br>(±0.5°C, 0.1°C)   | 20...90%<br>(±5%, 0.1%)  |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|[<img src=".github\images\sensors\DHT12\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DHT12-module.html)|
|DHT20/AM2108          |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\DHT20\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DHT20-module.html)|
|DHT21/AM2301          |-40...80°C<br>(±1.0°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|[<img src=".github\images\sensors\DHT21\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DHT21-module.html)|
|DHT22/AM2302          |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±2%, 0.1%)   |                             |[Single Wire](README.md#single-wire-dht11-and-etc)|[<img src=".github\images\sensors\DHT22\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DHT22-module.html)|
|DS18B20               |-55...125°C<br>(±0.5°C, 0.0625°C)|                          |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|[<img src=".github\images\sensors\Dallas\DS18B20.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DS18B20-module.html)|
|DS18S20 (DS1820)      |-55...125°C<br>(±0.5°C, 0.5°C)   |                          |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|[<img src=".github\images\sensors\Dallas\TO-92.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DS1820-module.html)|
|DS1822                |-55...125°C<br>(±2.0°C, 0.0625°C)|                          |                             |[1-Wire](README.md#1-wire-ds18b20-and-etc)|[<img src=".github\images\sensors\Dallas\TO-92.png" height="150"/>](https://www.aliexpress.com/w/wholesale-DS1822-module.html)|
|HDC1080               |-40...125°C<br>(±0.2°C, 0.1°C)   |0...100%<br>(±2%, 0.1%)   |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\HDC1080\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-HDC1080-module.html)|
|HDC2080               |-40...125°C<br>(±0.2°C, 0.1°C)   |0...100%<br>(±2%, 0.1%)   |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\HDC2080\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-HDC2080-module.html)|
|HTU21D(F)             |-40...125°C<br>(±0.3°C, 0.1°C)   |0...100%<br>(±2%, 0.04%)  |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\HTU21\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-HTU21D-module.html)|
|LM75                  |-55...125°C<br>(±2.0°C, 0.1°C)   |                          |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\LM75\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-LM75-module.html)|
|MAX31725              |-40...105°C<br>(±0.5°C, 0.004°C) |                          |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\MAX31725\general.png" height="150"/>](https://www.mlab.cz/module/LTS01/)|
|MAX31855              |-200...1800°C<br>(±2.0°C, 0.25°C)|                          |                             |[SPI](README.md#1-wire-ds18b20-and-etc)|[<img src=".github\images\sensors\MAX31855\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-MAX31855-module.html)|
|MAX6675               |0...1024°C<br>(±9.0°C, 0.25°C)   |                          |                             |[SPI](README.md#1-wire-ds18b20-and-etc)|[<img src=".github\images\sensors\MAX6675\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-MAX6675-module.html)|
|SCD30                 |0...50°C<br>(±0.4°C, 0.01°C)     |0...100%<br>(±3%, 0.004%) |0...40000 ppm CO₂<br>(±30 ppm, 1.0 ppm) |[I²C](README.md#ic)|[<img src=".github\images\sensors\SCD30\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SCD30-module.html)|
|SCD40                 |-10...60°C<br>(±0.8°C, 0.003°C)  |0...100%<br>(±6%, 0.002%) |400...2000 ppm CO₂<br>(±50 ppm, 1.0 ppm)|[I²C](README.md#ic)|[<img src=".github\images\sensors\SCD4x\SCD40.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SCD40-module.html)|
|SCD41                 |-10...60°C<br>(±0.8°C, 0.003°C)  |0...100%<br>(±6%, 0.00%2) |400...5000 ppm CO₂<br>(±40 ppm, 1.0 ppm)|[I²C](README.md#ic)|[<img src=".github\images\sensors\SCD4x\SCD41.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SCD41-module.html)|
|SHT20                 |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±3%, 0.04%)  |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT2x\SHT20.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT20-module.html)|
|SHT21                 |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±2%, 0.04%)  |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT2x\SHT21.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT21-module.html)|
|SHT25                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.04%)|                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT2x\SHT25.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT25-module.html)|
|SHT30/GXHT30          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%)  |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT3x\SHT30.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT30-module.html)|
|SHT31/GXHT31          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%)  |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT3x\SHT31.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT31-module.html)|
|SHT35/GXHT35          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.5%, 0.01%)|                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT3x\SHT35.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT35-module.html)|
|SHT40                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT4x\SHT40.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT40-module.html)|
|SHT41                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT4x\SHT41.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT41-module.html)|
|SHT43                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.01%)|                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT4x\SHT43.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT43-module.html)|
|SHT45                 |-40...125°C<br>(±0.1°C, 0.01°C)  |0...100%<br>(±1%, 0.01%)  |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHT4x\SHT45.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHT45-module.html)|
|SHTC3                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%) |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\SHTC3\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-SHTC3-module.html)|
|Si7021                |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±2%, 0.025%) |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\Si7021\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-Si7021-module.html)|
|TMP102                |-40...125°C<br>(±0.5°C, 0.06°C)  |                          |                             |[I²C](README.md#ic)|[<img src=".github\images\sensors\TMP102\general.png" height="150"/>](https://www.aliexpress.com/w/wholesale-TMP102-module.html)|

A comprehensive overview of the sensors can be found here (RU): https://kotyara12.ru/iot/th_sensors/
