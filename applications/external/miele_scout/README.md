# Miele Scout RX2 - Flipper Zero Remote

Flipper Zero app for controlling the Miele Scout RX2 robot vacuum over IR.

## Install

Grab the pre-built `dist/miele_scout.fap` from this repo and copy it to your Flipper Zero SD card:

```
SD Card/apps/Infrared/miele_scout.fap
```

Open it from **Apps > Infrared > Miele Scout RX2**.

## Usage

The app has two modes. Press **Back** to switch between them.

### Drive mode

Use the d-pad to steer the robot directly:

- **Up / Down / Left / Right** - move the robot
- **OK** - start cleaning

Point the Flipper's IR port at the robot. Release the button to stop.

### Menu mode

Scroll with **Up / Down**, press **OK** to send:

- **POWER** - turn on/off
- **BASE** - send to charging base
- **PLAY/PAUSE** - start or pause cleaning
- **OK** - confirm on robot display
- **MODE** - cleaning program
- **WIFI** - WiFi pairing
- **TIMER** - set timer
- **CLOCK** - clock settings
- **CLIMB** - climbing height (HI/LO)
- **MUTE** - sound on/off

Long-press **Back** to exit.

## Build from source

If you want to tweak the code and build it yourself, you need [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool):

```
pip install ufbt
```

Then just run `ufbt` in the project folder:

```
ufbt
```

This compiles `miele_scout.c` and outputs the app to `dist/miele_scout.fap`. Copy that to your Flipper and you're good to go.

The IR codes are defined at the top of `miele_scout.c` if you need to change any of them. The NEC protocol address is `0x01` and each button has its own command byte.

## Compatibility

Built for Flipper Zero official firmware. Uses NEC IR protocol. Works with any Miele Scout RX2 that has the standard IR remote.
