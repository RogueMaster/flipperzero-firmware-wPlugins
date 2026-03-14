![Flipper usage](https://user-images.githubusercontent.com/10090793/211182642-e41919c5-3091-4125-815a-2d6a77a859f6.png)
# Unitemp - Universal temperature sensor reader
[![GitHub release](https://img.shields.io/github/release/quen0n/unitemp-flipperzero?include_prereleases=&sort=semver&color=blue)](https://github.com/quen0n/unitemp-flipperzero/releases/)
[![License](https://img.shields.io/github/license/quen0n/unitemp-flipperzero)](https://github.com/quen0n/unitemp-flipperzero/blob/dev/LICENSE.md)
[![Build dev](https://github.com/quen0n/unitemp-flipperzero/actions/workflows/build_dev.yml/badge.svg?branch=dev)](https://github.com/quen0n/unitemp-flipperzero/actions/workflows/build_dev.yml)

[Flipper Zero](https://flipper.net/products/flipper-zero) application for reading temperature, humidity and pressure sensors like a DHT11/22, DS18B20, BMP280, HTU21 and more. 

## 🚧 Unitemp 2 is under construction 🚧
Development of the Unitemp app began in 2022. Much has changed since then: the Flipper Zero firmware API has stabilized, and new features and improvements have been added. The new version of Unitemp will update the code to reflect these changes, and accumulated bugs will be fixed. It is also planned to add a detailed description of connecting sensors to the Flipper Zero. 

❗You can already try out the updated app by installing it yourself or as part of the [Unleashed Firmware](https://web.unleashedflip.com/) dev builds. You can review the changes in [CHANGELOG.md](CHANGELOG.md). Please test the app and report bugs in [Issues](https://github.com/quen0n/unitemp-flipperzero/issues/new) or on [Discord](https://discord.com/channels/740930220399525928/1056727938747351060).

## How to install latest dev version
- Go to [Actions](https://github.com/quen0n/unitemp-flipperzero/actions), select last workflow run
<img width="1814" height="503" alt="image" src="https://github.com/user-attachments/assets/cc3c7459-c2a7-41f9-bd01-40a400757cca" />

- Click to download icon
<img width="1882" height="732" alt="image" src="https://github.com/user-attachments/assets/d08e3d74-447d-426c-b7a9-46a6ee8619b0" />

- Download the archive and unzip it
- Upload unitemp.fap to the flipper in SD Card/apps/GPIO via qFlipper, [Flipper lab](https://lab.flipper.net/archive), or mobile app
<img width="862" height="532" alt="image" src="https://github.com/user-attachments/assets/7d104513-21fc-4772-b405-982385f8ecd5" />

- Enjoy!



## List of supported sensors

| Model                | Temperature range<br>(accuracy, step)| Humidity range<br>(accuracy, step)| Extra range<br>(accuracy, step)| Interface     |Image and link |
|:--------------------:|:-------------------------------:|:------------------------:|:---------------------------:|:-------------:|:--------------:|
|AHT10                 |-40...85°C<br>(±0.3°C, 0.01°C)   |0...100%<br>(±2%, 0.024%) |                             |I2C            |[<img src=".github\images\sensors\AHT10\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-AHT10-module.html)|
|AHT20                 |-40...85°C<br>(±0.3°C, 0.01°C)   |0...100%<br>(±2%, 0.024%) |                             |I2C            |[<img src=".github\images\sensors\AHT20\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-AHT20-module.html)|
|AM2320                |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |Single Wire/I2C|[<img src=".github\images\sensors\AM2320\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-AM2320-module.html)|
|BME280                |-40...85°C<br>(±1.0°C, 0.01°C)   |0...100%<br>(±3%, 0.008%) |300...1100 hPa<br>(±1.0 hPa, 0.0016 hPa) |I2C            |[<img src=".github\images\sensors\BME280\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-BME280-module.html)|
|BME680                |-40...85°C<br>(±0.5°C, 0.01°C)   |0...100%<br>(±3%, 0.008%) |300...1100 hPa<br>(±0.6h Pa, 0.18 hPa)   |I2C            |[<img src=".github\images\sensors\BME680\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-BME680-module.html)|
|BMP180                |-40...85°C<br>(±0.5°C, 0.01°C)   |                          |300...1100 hPa<br>(±1.0 hPa, 0.01 hPa)   |I2C            |[<img src=".github\images\sensors\BMP180\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-BMP180-module.html)|
|BMP280                |-40...85°C<br>(±1.0°C, 0.01°C)   |                          |300...1100 hPa<br>(±1.0 hPa, 0.0016 hPa) |I2C            |[<img src=".github\images\sensors\BMP280\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-BMP280-module.html)|
|DHT11 (AOSONG)        | 0...50°C<br>(±2°C, 1.0°C)       | 20...90%<br>(±5%, 1.0%)  |                             | Single Wire   |[<img src=".github\images\sensors\DHT11\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT11-module.html)|
|DHT11 (ASAIR)         | -20...60°C<br>(±2°C, 0.1°C)     | 5...95%<br>(±5%, 1.0%)   |                             | Single Wire   |[<img src=".github\images\sensors\DHT11\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT11-module.html)|
|DHT12                 | -20...60°C<br>(±0.5°C, 0.1°C)   | 20...90%<br>(±5%, 0.1%)  |                             | Single Wire   |[<img src=".github\images\sensors\DHT12\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT12-module.html)|
|DHT20/AM2108          |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |I2C            |[<img src=".github\images\sensors\DHT20\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT20-module.html)|
|DHT21/AM2301          |-40...80°C<br>(±1.0°C, 0.1°C)    |0...100%<br>(±3%, 0.1%)   |                             |Single Wire    |[<img src=".github\images\sensors\DHT21\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT21-module.html)|
|DHT22/AM2302          |-40...80°C<br>(±0.5°C, 0.1°C)    |0...100%<br>(±2%, 0.1%)   |                             |Single Wire    |[<img src=".github\images\sensors\DHT22\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT22-module.html)|
|DS18B20               |-55...125°C<br>(±0.5°C, 0.0625°C)|                          |                             |One Wire       |[<img src=".github\images\sensors\Dallas\DS18B20.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DS18B20-module.html)|
|DS18S20 (DS1820)      |-55...125°C<br>(±0.5°C, 0.5°C)   |                          |                             |One Wire       |[<img src=".github\images\sensors\Dallas\DS1820.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DS1820-module.html)|
|DS1822                |-55...125°C<br>(±2.0°C, 0.0625°C)|                          |                             |One Wire       |[<img src=".github\images\sensors\Dallas\DS1822.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DS1822-module.html)|
|HDC1080               |-40...125°C<br>(±0.2°C, 0.1°C)   |0...100%<br>(±2%, 0.1%)   |                             |I2C            |[<img src=".github\images\sensors\HDC1080\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-HDC1080-module.html)|
|HDC2080               |-40...125°C<br>(±0.2°C, 0.1°C)   |0...100%<br>(±2%, 0.1%)   |                             |I2C            |[<img src=".github\images\sensors\HDC2080\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-HDC2080-module.html)|
|HTU21D(F)             |-40...125°C<br>(±0.3°C, 0.1°C)   |0...100%<br>(±2%, 0.04%)  |                             |I2C            |[<img src=".github\images\sensors\HTU21\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-HTU21D-module.html)|
|LM75                  |-55...125°C<br>(±2.0°C, 0.1°C)   |                          |                             |I2C            |[<img src=".github\images\sensors\LM75\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-LM75-module.html)|
|MAX31725              |-40...105°C<br>(±0.5°C, 0.004°C) |                          |                             |I2C            |[<img src=".github\images\sensors\MAX31725\general.png" width="150"/>](https://www.mlab.cz/module/LTS01/)|
|MAX31855              |-200...1800°C<br>(±2.0°C, 0.25°C)|                          |                             |SPI            |[<img src=".github\images\sensors\MAX31855\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-MAX31855-module.html)|
|MAX6675               |0...1024°C<br>(±9.0°C, 0.25°C)   |                          |                             |SPI            |[<img src=".github\images\sensors\MAX6675\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-MAX6675-module.html)|
|SCD30                 |0...50°C<br>(±0.4°C, 0.01°C)     |0...100%<br>(±3%, 0.004%) |0...40000 ppm CO2<br>(±30 ppm, 1.0 ppm) |I2C            |[<img src=".github\images\sensors\SCD30\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SCD30-module.html)|
|SCD40                 |-10...60°C<br>(±0.8°C, 0.003°C)  |0...100%<br>(±6%, 0.002%) |400...2000 ppm CO2<br>(±50 ppm, 1.0 ppm)|I2C            |[<img src=".github\images\sensors\SCD4x\SCD40.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SCD40-module.html)|
|SCD41                 |-10...60°C<br>(±0.8°C, 0.003°C)  |0...100%<br>(±6%, 0.00%2) |400...5000 ppm CO2<br>(±40 ppm, 1.0 ppm)|I2C            |[<img src=".github\images\sensors\SCD4x\SCD41.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SCD41-module.html)|
|SHT20                 |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±3%, 0.04%)  |                             |I2C            |[<img src=".github\images\sensors\SHT2x\SHT20.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SHT20-module.html)|
|SHT21                 |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±2%, 0.04%)  |                             |I2C            |[<img src=".github\images\sensors\SHT2x\SHT21.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SHT21-module.html)|
|SHT25                 |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.8%, 0.04%)|                             |I2C            |[<img src=".github\images\sensors\SHT2x\SHT25.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SHT25-module.html)|
|SHT30/GXHT30          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%)  |                             |I2C            |[<img src=".github\images\sensors\SHT3x\SHT30.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SHT30-module.html)|
|SHT31/GXHT31          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±2%, 0.01%)  |                             |I2C            |[<img src=".github\images\sensors\SHT3x\SHT31.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SHT31-module.html)|
|SHT35/GXHT35          |-40...125°C<br>(±0.2°C, 0.01°C)  |0...100%<br>(±1.5%, 0.01%)|                             |I2C            |[<img src=".github\images\sensors\SHT3x\SHT35.png" width="150"/>](https://www.aliexpress.com/w/wholesale-SHT35-module.html)|
|Si7021                |-40...125°C<br>(±0.3°C, 0.01°C)  |0...100%<br>(±2%, 0.025%) |                             |I2C            |[<img src=".github\images\sensors\Si7021\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-Si7021-module.html)|

A comprehensive overview of the sensors can be found here (RU): https://kotyara12.ru/iot/th_sensors/
## Gratitudes
- Special thanks [xMasterX](https://github.com/xMasterX), [vladin79](https://github.com/vladin79), [divinebird](https://github.com/divinebird), [jamisonderek](https://github.com/jamisonderek), [kaklik](https://github.com/kaklik)
- [Svaarich](https://github.com/Svaarich) for the UI design 
- [Unleashed firmware](https://github.com/DarkFlippers/unleashed-firmware) community for sensors testing and feedbacks
- ...and everyone who helped with development and testing
