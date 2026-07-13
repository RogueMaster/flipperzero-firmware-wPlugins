<!-- Source: https://docs.flipper.net/zero/infrared -->
<!-- Mirrored: 2026-07-14 -->

# Infrared

## Overview

The Flipper Zero features a built-in infrared module that allows it to interact with IR-enabled devices like televisions, air conditioners, and multimedia systems. Through this module, users can learn and store infrared remotes while also utilizing universal remotes to control other devices.

## Infrared Menu

The infrared application provides several key functions:

- **Universal remotes** — Cycles through known protocols and sends commands for all supported models using a dictionary stored on the microSD card (sometimes called a "brute force attack" due to iterating through the entire dictionary)
- **Learn new remote** — Records and saves signals from infrared remotes, with each button stored separately
- **Saved remotes** — Displays saved remotes for editing and playback
- **GPIO settings** — Configures signal output and power supply for external IR transmitters

## Hardware

The Flipper Zero's infrared module consists of:

- An IR light transparent plastic window
- Three transmitting infrared LEDs
- An infrared receiver

## Related Resources

- Reading infrared signals
- Using universal remotes
- Flipper Zero schematics
- Infrared application source code
