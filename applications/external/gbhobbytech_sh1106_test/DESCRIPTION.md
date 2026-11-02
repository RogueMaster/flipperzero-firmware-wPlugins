# SH1106 Test

Test and diagnose external 128x64 SH1106 I2C OLED displays
using your Flipper Zero GPIO interface.

## Features

- Ten OLED test patterns
- Pixel Walk and FPS benchmark
- Adjustable contrast and brightness
- Horizontal and vertical display flipping
- Display inversion and power control
- Software scrolling with adjustable speed
- Software fade and blink effects
- I2C scanner and address detection
- Clock and system status display
- Botanical Obligation mini-application

## Wiring

- GND to Flipper GND (pin 18)
- VCC to 3.3V (pin 9), if supported by your module
- SCL to C0 (pin 16)
- SDA to C1 (pin 15)

Use 3.3V I2C logic. Check your OLED module's specifications
before connecting it.

## Compatibility

Designed for SH1106 128x64 I2C OLED modules using addresses
0x3C or 0x3D.

Hardware-validated on an external SH1106 display.

## Credits

Adapted from [SSD1306 Test](https://github.com/martinbogo/ssd1306_test)
by Martin Bogomolni.

SH1106 adaptation maintained by gbhobbytech.
