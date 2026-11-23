# Moon Lander for Flipper Zero

Clauded from scratch to test Claude's abilities, tuned & tweaked by FToThe3rdPower.

## Requirements

- Any [Flipper Zero](https://flipper.net/products/flipper-zero) on firmware 1.5.1 (the current release). Developed on 1.4.3; also builds against 1.5.1-rc.
- The Vidya tilt modes need the Video Game Module, aka the 'VGM': [Flipper Zero Video Game Module](https://flipper.net/products/video-game-module-for-flipper-zero).

## Build & install
### Build from source
```sh
pip install --upgrade ufbt    # one time
ufbt                          # builds .fap into dist/
ufbt launch                   # build, upload, and run on the connected Flipper
```

Make sure qFlipper and lab.flipper.net aren't hogging the serial port when
trying to run `ufbt launch`.

### Tunables
The top of `game.c` has a `#define` block for gravity, thrust, rotation rate,
the tilt-thrust dead-zone, and audio frequencies. The safe-landing thresholds
for each difficulty are set in `apply_difficulty()` in `game.c`.

### Run without building
Transfer `dist/moon_lander.fap` directly to your Flipper and run it from the
Apps menu.

## Controls

### Button modes
The thrust button is Up; Settings → Thrust button switches it to OK.

| Input | Action |
|-------|--------|
| Left / Right (hold) | Rotate lander |
| Thrust button — Button Binary | Hold = full thrust |
| Thrust button — Button Tap Impulse | Each press = fixed velocity kick |
| Thrust button — Button Ramp | Hold; thrust ramps 0→100% over ~0.5 s |
| OK on banner | Next level (landed) / Retry (crashed) |
| Back (tap) while flying | Pause menu |
| Back (hold 1 s) while flying or paused | Return to menu |
| Back on banner or popup | Return to menu |

### Pause menu
Tapping Back mid-flight freezes the game and opens the pause menu. Up and
Down choose, OK picks, and another Back tap resumes. **Zero tilt** (tilt
modes only) makes the way you're holding the Flipper the new "straight up"
for steering, then resumes. **Quit to menu** keeps a new high score, like
holding Back does.

### 'Vidya' (VGM tilt) modes
Requires the Flipper Zero Video Game Module. Tilt left/right steers the lander
(device roll → lander angle, 1:1 mapping). Calibration captures the "upright"
position at the start of each level, including after retries. If the zero
drifts mid-flight, tap Back, hold the Flipper upright and pick Zero tilt.

| Mode | Thrust |
|------|--------|
| Vidya Tilt + Tap | The thrust button fires a burst |
| Vidya Tilt + Binary | Hold the thrust button |
| Vidya Tilt + Ramp | Hold the thrust button, ramps up |
| Vidya Full Tilt | Tilt forward = proportional thrust; no button needed |

## Difficulty

| Difficulty | Vy limit | Vx limit | Angle limit |
|------------|----------|----------|-------------|
| Easy       | < 16     | < 8      | < 25.2°     |
| Medium     | < 8      | < 4      | < 12.6°     |
| Hard       | < 4      | < 2      | < 6.3°      |
| Realistic  | < 1      | < 1      | < 3.0°      |
| Custom     | < 1–16   | < 1–8    | < 1–25°     |

For Custom, scroll the Difficulty row in Settings to Custom, then press OK to
set each limit (it can't be looser than Easy).

Pads are the flat strips with a 1×, 2×, 3× or 5× label, and only a pad's own
width is flattened (3× and 5× pads are narrower). Touching down anywhere else,
even on a naturally flat spot, crashes, and the banner says "Missed the pad".

## Seeds
Settings → Seed picks the world. Each seed from 1 to 9999 generates its own
30 levels (terrain, pads and starting drift); seed 1 is the original set.
Left/Right steps through seeds and OK picks a random one. The high score
screen shows the seed the high score was set on.

## TV mode
The Video Game Module always outputs 4:3 and shows each Flipper pixel 2 wide
by 3 tall, so the game looks stretched on a TV. Settings → TV mode draws the
playfield (terrain, lander, pads) at 2/3 height to compensate; physics is
unchanged. Text isn't squished.

The module's own firmware sets that picture, and an app can't change it: the
Flipper only streams its 128×64 frame. The firmware
([video-game-module](https://github.com/flipperdevices/video-game-module),
`app/frame.c`) always sends 640×480 at 60 Hz, the one mode every HDMI display
must accept, and repeats each Flipper row on 3 lines to fill more of the
screen.

In-game frames are drawn at ~31 fps (~12 fps while a banner or popup is up);
physics still runs at 60 Hz. Each frame is sent to the TV over a serial link,
and drawing fewer of them leaves the Flipper time for input.

## Pads per level

| Level | Pads |
|------:|-----:|
| 1–3   | 5    |
| 4–6   | 4    |
| 7–8   | 3    |
| 9     | 2    |
| 10–30 | 1    |

Pad multipliers are distance-based — 1× directly below spawn, 2× nearby,
3× mid-range, 5× near screen edges. Pad widths match difficulty: 1× and 2×
pads are 16 px wide; 3× pads are 13 px; 5× pads are 10 px (a few px wider
than the lander's 7 px footprint).

## Debug HUD
Turn it on in Settings. It replaces the normal HUD while flying (the pause
menu and the landed/crashed banner hide it):

| Field | Meaning |
|-------|---------|
| `HP` | Free heap (bytes) |
| `TK` | Game ticks processed (resets when you return to the menu) |
| `Q` | Event queue depth (current / peak) |
| `DR` | Longest frame draw in the last second (ms) |
| `TG` | Longest gap between game ticks in the last second (ms); ~16 is normal |
| `A` `X` `Y` `S` | Angle (°), Vx, Vy, and speed |

With Debug HUD on, everything except speed is also logged at 6 Hz under
`LanderDbg` which can be accessed via `ufbt cli`, then `log`

## Project layout
| File | Purpose |
|------|---------|
| `moon_lander.c` | Main loop, event dispatch, 60 Hz tick, screen transitions |
| `moon_lander.h` | Shared enums — ThrustMode, FuelMode, Difficulty, Screen |
| `menu.c / .h` | Title/menu screen and mode selectors |
| `pause_menu.c / .h` | Pause menu rows and drawing; `game.c` decides what each choice does |
| `game.c / .h` | Physics, terrain, collision, audio, drawing |
| `lander_sprite.c / .h` | Lander silhouette — static and rotated with flame |
| `vgm_tilt.c / .h` | VGM IMU wrapper (pitch/roll → steer/thrust) |
| `sensors/` | ICM-42688P driver and IMU fusion (VGM tilt hardware layer), copied unchanged from [flipperzero-game-engine](https://github.com/flipperdevices/flipperzero-game-engine) at commit 8946381 (GPL-3.0); its Madgwick filter is based on Sebastian Madgwick's algorithm via [MadgwickAHRS](https://github.com/arduino-libraries/MadgwickAHRS) (GPL-3.0) |
| `application.fam` | Flipper app manifest (name, version, short description, icon) |
| `docs/description.md` | App description shown in the Flipper Apps Catalog (limited Markdown) |
| `changelog.md` | Version history, also shown in the catalog |