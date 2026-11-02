# SH1106 Test for Flipper Zero

An OLED diagnostic application for Flipper Zero, designed to
test external SH1106 128x64 I2C displays.

Adapted from SSD1306 Test by Martin Bogomolni (martinbogo).
SH1106 adaptation maintained by gbhobbytech.

## Features

- Ten display test patterns
- Full-screen pixel on/off tests
- Checkerboard, lines, borders and gradients
- Text and geometric shape rendering
- Adjustable display contrast
- Display inversion and power control
- Horizontal and vertical display flipping
- Software horizontal scrolling with adjustable speed
- Software fade and blink effects
- Start-line adjustment
- I2C address detection and scanner
- Pixel Walk diagnostic
- FPS benchmark
- Clock and Flipper status display
- Botanical Obligation mini-application

## Supported Hardware

Designed for SH1106-based 128x64 OLED modules using I2C.

Supported display addresses:
- 0x3C
- 0x3D

The SH1106 controller has 132 display RAM columns.
This application uses page-based addressing and a two-column
offset for the typical 128-pixel visible area.

Horizontal mirroring is implemented in software for reliable
operation with the tested module.

## Wiring

Connect the OLED to the Flipper Zero GPIO header.

| OLED | Flipper Zero | GPIO pin |
|------|--------------|----------|
| GND  | GND          | 18       |
| VCC  | 3.3V         | 9        |
| SCL  | C0           | 16       |
| SDA  | C1           | 15       |

IMPORTANT: Flipper Zero GPIO uses 3.3V logic.

Confirm the voltage requirements of your particular OLED module.
Do not connect I2C lines pulled up to 5V directly to the Flipper.

## Building

Requires the Flipper Zero uFBT build environment.

Clone the repository:

    git clone https://github.com/gbhobbytech/ssd1306_test.git
    cd ssd1306_test
    git checkout sh1106-development

Select the official firmware SDK:

    ufbt update --channel=release

Build:

    ufbt

Build and launch on a connected Flipper Zero:

    ufbt launch

Close qFlipper or other software using the serial connection
before launching through uFBT.

## Testing

The SH1106 adaptation has been tested on physical hardware.

Validated functionality includes:
- Initialisation and page addressing
- All ten display patterns
- Horizontal and vertical orientation
- Contrast and inversion
- Display on/off
- Software scrolling and speed adjustment
- Software fade and blink
- Pixel Walk and FPS benchmark
- I2C scanning
- Clock and status display
- Exit and relaunch
- External display disconnection and reconnection

Testing on additional SH1106 modules is welcome.

## Application Identity

Name: SH1106 Test
App ID: gbhobbytech_sh1106_test
Category: GPIO
Maintainer: gbhobbytech

## Original Project and Acknowledgements

SH1106 Test is a derivative of SSD1306 Test, originally
developed by Martin Bogomolni (martinbogo).

Original source:
https://github.com/martinbogo/ssd1306_test

This adaptation retains the original application's design
and diagnostic features while implementing SH1106-specific
display initialisation, page addressing, horizontal mirroring,
software scrolling and software fade/blink.

Thanks to Martin Bogomolni for making the original project
available to the community.

## Licensing

The original project's README identifies its licence as MIT.

The original repository did not include a standalone LICENSE
file when this adaptation was prepared.

The complete applicable copyright and licence notice must be
confirmed and retained before distributing the derivative.

See the original project for licensing information.

## Status

Version 0.5 - Hardware-validated release candidate.

Documentation and official Flipper Application Catalog
submission preparation are in progress.
