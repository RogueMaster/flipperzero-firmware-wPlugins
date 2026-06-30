# Polk MagnifiMini Remote

A Flipper Zero IR remote for the **Polk MagnifiMini soundbar**.

## Screenshots

![Remote controls](images/screenshots/screen_controls.png)
![Audio Input Sources](images/screenshots/screen_audio_sources.png)
![Audio EQ Presets](images/screenshots/screen_eq_presets.png)

## Features

- Power, Mute, Night mode
- Volume Up / Down
- Playback controls (Previous, Play, Next)
- Voice level Up / Down
- Bass level Up / Down
- Source selection: AUX, Optical, HDMI ARC
- EQ Preset selection: Cinema, Sports, Music

## Controls

Use **Up/Down** to move between rows. Use **Left/Right** to trigger the action on that side (e.g. Volume Down / Volume Up). Press **OK** to trigger the centre action (e.g. Play, Power).

| Row | Left | OK | Right |
|---|---|---|---|
| FIRST | Night mode | Power on/off | Mute |
| VOL | Volume Down | — | Volume Up |
| PLAYBACK | Previous | Play/Pause | Next |
| VOICE | Voice Down | — | Voice Up |
| BASS | Bass Down | — | Bass Up |
| SOURCE | — | Open source menu | — |
| EQ PRESET | — | Open EQ menu | — |

For **SOURCE** and **EQ PRESET**, press OK to open the selection menu, then use Up/Down to choose an option and OK to confirm. Press Back to return.

## Building

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

```sh
ufbt          # build only
ufbt launch   # build + deploy via USB
```

## IR Signals

Raw IR signals are in `Polk_MagnifiMini.ir` (NECext protocol, address `0xC8 0x91`). This file can be used directly with the Flipper Zero built-in IR app if you prefer a simpler remote without the custom UI.

## Compatibility

Tested with the **Polk MagnifiMini** soundbar (model MagnifiMini). May work with other Polk MagnifiMini variants that share the same IR protocol.
