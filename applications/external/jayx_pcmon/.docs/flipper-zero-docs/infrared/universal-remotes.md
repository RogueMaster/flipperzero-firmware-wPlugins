<!-- Source: https://docs.flipper.net/zero/infrared/universal-remotes -->
<!-- Mirrored: 2026-07-14 -->

# Using Universal Remotes

## Overview

Flipper Zero includes built-in universal infrared remotes that allow you to control various devices without copying their original remotes. The device emulates a dictionary of IR protocols used by major electronics brands.

## Available Universal Remotes

The universal remotes can be accessed via **Main Menu > Infrared > Universal Remotes**:

- **TV Remote**: Control power, volume, channels, and mute
- **Audio System Remote**: Control power, play/pause, skip tracks, volume, and mute
- **Projector Remote**: Control power, volume, and mute
- **Air Conditioner Remote**: Control power, dehumidifier mode, and temperature settings (cooling/heating)

## Sending Commands

To send a command:

1. Navigate to **Main Menu > Infrared > Universal Remotes**
2. Select the appropriate remote for your device
3. Choose the desired button/command
4. Point Flipper Zero at the target device
5. Press the OK button to send the command
6. Keep the device pointed until the command executes or the protocol dictionary completes

## Contributing Missing Remotes

If a device isn't supported, you can capture its IR signals and submit them to the community:

1. Capture and save IR signals using Flipper Zero
2. Download the captured file to your computer via qFlipper
3. Open the file in a text editor
4. Fork the FlipperDevices/FlipperZero firmware repository
5. Append the data to the corresponding file (tv.ir, ac.ir, projector.ir, or audio.ir) in `applications/main/infrared/resources/infrared/assets`
6. Commit changes and submit a pull request

Once approved, your signals become available to the entire community.
