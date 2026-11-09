# IR Jammer for Flipper Zero

IR jammer app that works with the internal IR LED and external IR module.

<img src="Screenshot.jpg" width="350" alt="Screenshot">

## What it does

The app floods the area with infrared pulses, which drowns/blockes out the IR
signal from remote controls and other IR receivers.

It can affect any device that uses IR, for example TVs, projectors, AC units,
soundbars, IR cameras and night vision equipment, and IR based detection or sensor
systems.

**NOTE**: Random and Empty modes with Density set high will run the LED close to continuously. 
Avoid running them for long periods, as the ext module can get warm.

## Features

- 5 modes: Basic, Enhanced Basic, Sweep, Random, Empty
- Output: Auto / Internal / External (external is used when attached)
- Live stats: jams, time, jams per second

## Settings

- **Mode**: Basic, Enhanced Basic, Sweep, Random, Empty is different ways of jamming
- **Freq**: carrier frequency, used by Random and Empty modes only
- **Output**: Auto, Int or Ext = which IR output to use
- **Timing / Mark / Space**: pulse length in microseconds sets the carrier frequency in Basic, Enhanced and Sweep
- **Min / Max / Speed**: range and step size for Sweep mode
- **Density**: how much IR is sent, higher means closer to continuous jamming


## Controls

- **Up / Down**: select setting
- **Left / Right**: change value
- **OK**: start / stop
- **Back**: stop, or exit when idle

## Disclaimer

For use on your own devices and testing only. Jamming other people's remotes without
permission may be illegal where you live.

## Source

https://github.com/Clawzman/flipper-ir-jammer

## Credits

Ported from the IR jammer in Bruce firmware made by Thoxy67
