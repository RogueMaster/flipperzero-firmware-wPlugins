<!-- Source: https://docs.flipper.net/zero/sub-ghz/read-raw -->
<!-- Mirrored: 2026-07-14 -->

# Reading RAW signals - Flipper Zero Documentation

## Overview

The Flipper Zero can record radio signals in raw format without decoding them, functioning similarly to a dictaphone. This feature allows users to work with both known and unknown protocols for later playback and analysis.

## How to Read Raw Signals

To record and save a signal in raw format:

1. Navigate to Main Menu > Sub-GHz
2. Select "Read Raw" and press Rec to begin recording
3. Press the button on the remote control to record
4. Press Stop, then Save
5. Name the recorded signal and confirm

## Read Raw Configuration Menu

Users can adjust settings by pressing Config on the scanning screen:

**Frequency Configuration**
- Switch frequencies using left/right controls
- Manually select from available frequency options
- Use the frequency analyzer feature to determine the correct remote frequency

**Modulation Configuration**
- The device requires proper modulation settings since it's not a software-defined radio
- Supported modulations include:
  - AM270: amplitude modulation, 270 kHz bandwidth
  - AM650: amplitude modulation, 650 kHz bandwidth (default)
  - FM238: frequency modulation, 270 kHz bandwidth
  - FM476: frequency modulation, 270 kHz bandwidth

**RSSI Threshold Configuration**
- Set minimum signal strength sensitivity in dBm units
- Recording pauses when signal drops below the threshold
- Recording resumes when signal strength recovers

## Sending Raw Signals

To transmit a saved raw signal:

1. Go to Main Menu > Sub-GHz > Saved
2. Select the desired signal
3. Press Send

Transmission is limited to frequencies permitted in your region.
