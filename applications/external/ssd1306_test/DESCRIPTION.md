# SSD1306 Test

Test and diagnose external SSD1306 128x64 I2C OLED displays. Supports monochrome and yellow-bar (dual-color) variants.

## Features

- Clock and status board with battery, date, and device name
- Test patterns: solid fills, checkerboard, lines, border, color zones, page fill, gradient, shapes, text
- Adjustable contrast (0 to 255)
- Invert, flip, all-pixels-on, power on/off, variant toggle
- Hardware horizontal and diagonal scrolling
- Auto-detects I2C address (0x3C and 0x3D)

## Wiring

- GND to GND (pin 18)
- VCC to 3.3V (pin 9)
- SCL to C0 (pin 16)
- SDA to C1 (pin 15)

**Note:** Use 3.3V, not 5V. The Flipper GPIO is 3.3V logic. Most modules run fine at 3.3V. If your module needs 5V for power, use the 5V pin (pin 1) for VCC only; SCL and SDA must stay 3.3V-tolerant.
