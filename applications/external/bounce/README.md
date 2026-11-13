# Bounce for Flipper Zero

A tribute to the classic Nokia *Bounce*: roll a ball through brick levels,
jump through every ring to open the exit gate, and avoid thorns and spikers.

## Controls

| Button        | Action                                             |
|---------------|----------------------------------------------------|
| Left / Right  | Roll                                               |
| Up / OK       | Jump (hold to keep bouncing)                       |
| Down          | Dive while in water                                |
| Back          | Pause (Resume / Restart level / Title screen)      |

On the title screen, Left/Right picks any unlocked level, Up/Down switches
between the level and sound options, and Back exits the app.

## Gameplay

- **Rings**: roll or jump through the middle of each ring. When every ring is
  collected, the exit gate opens.
- **Checkpoints** (flags): lose a ball and you respawn at the last one you touched.
- **Springs**: launch you about five tiles high.
- **Water**: the ball floats. Hold Down to dive, and press Jump at the surface to leap out.
- **Spikes and spikers**: touching them pops the ball.
- **Hearts**: an extra ball.

You start with 3 balls. Progress (unlocked levels), best score and the sound
setting are saved to `/ext/apps_data/bounce/bounce.save`.

## Building

Requires [uFBT](https://github.com/flipperdevices/flipperzero-ufbt).

```sh
ufbt              # builds dist/bounce.fap
ufbt launch       # builds, installs and starts it on a USB-connected Flipper
```

Or copy `dist/bounce.fap` to `/ext/apps/Games/` on the SD card.

## Adding levels

Levels are ASCII maps in [levels.h](levels.h). Each char is an 8x8 tile, and
the legend is at the top of that file. Append a `levelN_rows` array and add it
to the `levels[]` table. Levels can be up to 128x16 tiles.

Keep these physics limits in mind when designing:

- A jump climbs 2 tiles and clears a gap of up to 3 tiles.
- A spring climbs 5 tiles.
- Give the player a way back (steps or a spring) wherever they might miss a ring.
