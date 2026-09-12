# Application Submission

BEEPBACK is a memory game. The Flipper plays a sequence of tones and you play
it back, one step longer each round — then it starts changing the rules on you.

- **CLASSIC** — the sequence grows a step at a time, three lives.
- **RULES** — every round hands you a rule to obey. `SKIP DOWN` means the
  sequence plays DOWN and you don't press it. Also SWAP, BACKWARDS, NO DOUBLES,
  EVERY OTHER, DOUBLE, LAST TWICE. The rule changes when the round clears.
- **REFLEX** — one cue at a time against a draining window that shrinks with
  every hit. One miss ends the run.
- **CHALLENGE** — pick a rule and keep it, on one endless sequence.
- **DAILY** — one seeded run a day, identical on every Flipper, resetting at
  midnight.

Four assists — tone only, LED, on-screen shapes, on-screen arrows — each keeping
its own records, plus optional haptics that pulse the motor once per note of
every pattern, which makes the game playable with the volume at zero.

Scoring is ten points a note and a hundred for clearing a round, with no
multiplier. Records are kept per mode, time, speed and assist.

First submission, v1.0.

# Extra Requirements

None. No hardware add-ons, no external files, nothing written outside
`/ext/apps_data/beepback/`.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

Validated with the full `tools/bundle.py` rather than `--nolint`, so the
`ufbt lint` stage ran too.

One thing I should flag rather than have you find: **the screenshots are
rendered, not qFlipper captures.** The repository contains a tool
(`tools/shoot/`) that sets up the firmware's own copy of u8g2 with the two
fonts `canvas_set_font()` selects, points it at a 128x64 buffer, implements the
canvas API as `applications/services/gui/canvas.c` does, and calls the app's
real draw function. The output is what the device draws, at the required 4x.
Happy to replace them with qFlipper captures if you would rather.

# AI usage disclosure (Fill this out)

- Fully AI generated - explain what all the generated code does in moderate detail.

The game is mine: I designed it and wrote the original browser version, which
is where the rules, the scoring, the seeded generator and the screen layouts
come from. The Flipper port in this repository — all of the C — was written by
Claude (Opus 5) working from that browser build and under my direction, with me
testing each build on hardware and deciding the design questions as they came
up.

What the code does, by file:

- `beepback_app.c` — the only file that touches the Flipper API. Main loop,
  view port, input queue, speaker, LED and vibro.
- `beepback_game.c` — the run: scene transitions, playback timing, scoring,
  records, the challenge growth loop and the reflex ramp.
- `beepback_rules.c` — the seven rules as pure transforms of a sequence, the
  guard that rejects a rule that would leave nothing to press, and the
  mulberry32 generator the daily is seeded from.
- `beepback_nav.c` — input handling; one table decides where every screen goes
  when you leave it.
- `beepback_draw.c` — every screen, drawn with canvas primitives.
- `beepback_save.c` — a hand-packed little-endian save block with a version
  byte and a checksum.
- `beepback_tables.c`, `beepback_intro.c`, `beepback_led.c` — constant tables,
  the splash animation, and the five button colours.

The game logic touches no Flipper API — time enters through a tick function and
hardware leaves through plain fields — so it runs on a host machine under a
test suite of 253 checks in `test/`, which includes a fake canvas that fails
the build if any screen draws outside 128x64, if a string is wider than the
screen, or if one string is drawn through another.

# Reviewer Checklist (Don't fill this out, and don't remove it from the template)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
