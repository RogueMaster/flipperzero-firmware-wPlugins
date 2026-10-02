# Step Seq

A step sequencer for the [Flipper Zero](https://flipperzero.one)'s piezo speaker, with a melody grid and a drum row.

![The sequencer grid](screenshot.png)

## What it does

- **Melody:** one note per step across 15 pitch rows. Rows follow the chosen scale, so everything stays in key.
- **Drums:** a kick, snare and hi-hat on the bottom row, made from square-wave sweeps and noise.
- **One voice:** the speaker can only play one tone at a time, so a drum takes the first few milliseconds of its step and the note gets the rest.
- **Patterns:** eight slots, each 16 or 32 steps, with its own tempo, swing, scale, root and octave.
- **Scales:** minor and major pentatonic, minor, major, blues and chromatic, in any root.
- **Saving:** all patterns are saved to the SD card when you quit.

## Controls

| Button | Action |
| --- | --- |
| D-pad | Move the cursor; up/down also previews the pitch |
| OK | Place or remove a note; on the drum row, cycle kick, snare, hi-hat, off |
| Back | Play / stop |
| Hold OK | Open settings |
| Hold Back | Save and quit |

On the drum row a filled block is a kick, a hollow block is a snare and a dash is a hi-hat. In a 32-step pattern the screen shows 16 steps at a time and follows the cursor; the two blocks in the top bar show which half you are on.

## Settings

| Setting | Values |
| --- | --- |
| Pattern | 1 to 8 |
| Tempo | 60 to 240 bpm |
| Swing | Off, or 54% to 70% |
| Length | 16 or 32 steps |
| Scale, Root, Octave | The key the pitch rows follow |
| Volume | 1 to 5, shared by all patterns |
| Copy to next | Copies this pattern into the next slot and switches to it |
| Clear pattern | Empties the current pattern |

Use Left/Right to change a value. Copy and Clear run when you press Right.

## Build and install

You need [ufbt](https://github.com/flipperdevices/flipperzero-ufbt), the Flipper app build tool:

```bash
python3 -m pip install --upgrade ufbt
```

With the Flipper connected over USB (and qFlipper closed), build, install and start the app:

```bash
ufbt launch
```

It installs to `Apps → Media → Step Seq`. To build without a device, run `ufbt` and copy `dist/step_seq.fap` to `apps/Media` on the SD card.

## License

[MIT](LICENSE)
