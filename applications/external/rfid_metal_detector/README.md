# Flipper Zero RFID Metal Detector

A Flipper Zero application that repurposes the 125kHz RFID antenna to act as a makeshift metal detector. 

## Features
- **Visual & Audio Feedback**: Screen bar graph, increasing pitch, vibration, and LED alerts when metal is near.
- **Adjustable Sensitivity**: Tune out environmental noise or adjust for specific uses (like scanning a human) with the Left/Right buttons.
- **Multiple Modes**: Toggle Sound/Vibro modes with the OK button.

## Controls
- **Left / Right**: Adjust sensitivity (decrease / increase).
- **OK**: Cycle through alert modes (Sound+Vibro, Sound only, Vibro only, None).
- **Up / Down**: Open and close the Help screen.
- **Back**: Exit the application.

## How It Works

Instead of reading RFID tags, the app repurposes the 125kHz antenna to act as a raw induction coil. By keeping the electromagnetic field active, the app continuously measures the duration (pulse width) of the signal's positive half-waves using timer captures. When metal enters the field, it creates eddy currents that draw energy from the coil, causing the antenna's voltage/amplitude to drop. This drop alters the comparator's output, shortening the measured pulse width or causing it to drop out entirely. The code calculates the delta between a fixed baseline and a moving average of these real-time timing values to trigger the detection UI.

## Limitations

Because this app uses the RFID hardware for an unintended purpose, its range and accuracy are limited. Expect a detection range of only a few cm at most. This tool is primarily educational and experimental, and it is not a replacement for a dedicated metal detector.

## Installation

The easiest way to install is to download the pre-compiled `.fap` file from the Releases page and drop it directly onto your Flipper's SD card in the `apps/Tools/` folder.

### Build from Source
1. Clone this repository into your Flipper firmware (`applications_user/rfid_metal_detector`) or an empty directory for `ufbt`.
2. Compile with `ufbt` or `./fbt fap_rfid_metal_detector`.
3. Transfer the compiled `.fap` to your Flipper's `apps/Tools/` folder.

## Credits & License
Created by AI (with Tommy's assistance).
Licensed under the MIT License.
