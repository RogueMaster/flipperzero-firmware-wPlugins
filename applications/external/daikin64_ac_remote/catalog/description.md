# Daikin64 AC Remote

Daikin64 AC Remote is a Flipper Zero infrared remote for Daikin air conditioners that use the Daikin64 protocol, including APGS02-style remotes.

The app generates Daikin64 packets dynamically instead of replaying stored raw captures.

## Features

- Power toggle
- Mode selection: Auto, Cool, Heat, Dry, Fan
- Temperature control from 16 C to 30 C
- Fan control: Auto, Low, Medium, High, Turbo, Quiet
- Swing, Sleep, Turbo, Quiet, and timer controls
- Five favorite presets
- Saved last-used state and favorites on the SD card

## Usage

Use Left and Right to change pages, Up and Down to move between controls, and OK to activate the selected control.

On the Favorites page, press OK on a slot to send it. Long-press OK on a slot to save the current state into that slot.

Before listing an AC model as compatible, test the app with your indoor unit and original remote. Optional functions such as Sleep, Quiet, and Turbo may be ignored by some Daikin models even when the protocol frame is valid.
