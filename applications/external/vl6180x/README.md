# VL6180X

This is a simple app the makes uses of the [Adafruit VL6180X](https://www.adafruit.com/product/3316) distance sensor.

The code for configuring and reading from the VL6180X is a port of the [Circuit Python](https://github.com/adafruit/Adafruit_CircuitPython_VL6180X) code provided by Adafruit. There is also a [C++ library](https://github.com/adafruit/Adafruit_VL6180X) for use with an Arduino. 

This project does not work with the [Adafruit VL53L0X](https://www.adafruit.com/product/3317) distance sensor, but could be adapted to work with it.

# Connections

| Flipper Port | STEMMA QT Color | VL6180X PIN |
| ------------ | --------------- | ----------- |
| 8 (GND)      | Black           | GND         |
| 9 (3v3)      | Red             | VIN         |
| 15 (SDA)     | Blue            | SDA         |
| 16 (SCL)     | Yellow          | SCL         |

Note: The sensor's VIN line could be connected to the 5V line on the Flipper.
