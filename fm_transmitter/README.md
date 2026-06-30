# flipperzero-kt0803-transmitter
Use KT0803-type chips to transmit signal from module AUX port

## Warning
FM Transmitters are illegal if they are too powerful. Use in well-shielded environment or do not use it for too long.

## How it works
Check original FMTX library from Elechouse, a CPP library for Arduino

## Why I made it
I wanted to listen music through FM receiver, I didn't want to buy FM Transmitter, why tho, if I have a flipper?

## How to use
1. Install app by dropping .fap to /ext/apps/GPIO
2. Connect module using this pinout
   | Elechouse KT0803L | Flipper |
   |:------------------|--------:|
   | Pin GND (Ground)  | 8/11/18 |
   | Pin VCC (USE 3.3) | 9 (3V3) |
   | Pin SDA (I2C pin) | 15 (C1) |
   | Pin SCL (I2C pin) | 16 (C0) |

3. Open the app, select region and frequency
4. Click Init
5. Connect AUX on module to audio output (like microphone, phone, PC or MP3 player)

Do not disconnect module from flipper, because flipper is a power supply, use 3V3 (shared with SD, may interfere with SD card) or PC3 (for SD card protection, use flashlight app), you can connect flipper to a charger

> [!TIP]
> Solder antenna wire on ANT pin to extend signal range.

## Credits
- Elechouse for basic init module library.
- @victormico for TEA5767 app and @NaejEL for I2C tools, they are well examples for furi_hal.h i2c features.
