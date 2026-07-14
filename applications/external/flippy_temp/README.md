# Flippy Sensors 🐬

flippy sensor is a WIP multi-sensor board for the flipper zero. It includes a SHT30 for temperature and humidity, a LSM6DS3TR-C for gyro and accelerometer and a PA1010D for GPS. A first batch is currently in production at JLCPCB.

It comes with an application to visualise data and log them on the SD card 💾 !

## The application
![App](assets/flippy_sensors_app.jpg)
**video demo :** [here](https://x.com/rqjdupont/status/1815095288660656223)

## PCB renders
![Front](assets/flippy_multisensor.png)
![Back](assets/flippy_multisensor2.png)

## sensors list
- **PA1010D**: GPS/GNSS [datasheet](https://cdn-learn.adafruit.com/assets/assets/000/084/295/original/CD_PA1010D_Datasheet_v.03.pdf?1573833002)
- **LSM6DS3TR-C**: Accelerometer and gyroscope [datasheet](https://www.mouser.fr/datasheet/2/389/lsm6ds3tr_c-1761429.pdf)
- **SHT30**: Temperature and humidity [datasheet](https://www.mouser.com/datasheet/2/682/Sensirion_Humidity_Sensors_SHT3x_Datasheet_digital-971521.pdf)

All sensors communicate with I2C
| sensor      | I2C address |
| ----------- | ----------- |
| PA1010D     | `0x10`      |
| LSM6DS3TR-C | `0x6A`      |
| SHT30       | `0x44`      |
