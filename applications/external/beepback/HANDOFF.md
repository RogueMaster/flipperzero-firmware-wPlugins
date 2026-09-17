# BEEPBACK firmware — handoff

## Paste this into Claude Code to start

> I'm porting a game called BEEPBACK to the Flipper Zero. The logic layer is
> already written and tested; I need the state machine, screens, input and save
> file built on top of it. Read HANDOFF.md in this folder first — it has the
> full spec, the exact constants, the screen layouts and the navigation map.
> Work in the order listed under "What's left". After each stage, run
> `./test/run_tests.sh` and make sure it still passes, and add tests for what
> you added. Don't change any constant without telling me, because a browser
> build has to stay identical to this one.

Everything below is for Claude Code, not for you.

---

## What this is

A memory and reaction game. Five modes. There are two builds that must behave
identically: a browser prototype (already finished, not in this folder) and this
Flipper firmware. Every timing and scoring number was settled in the browser
build and simulated for balance. **Treat the constants as fixed.**

## What already exists here

| file | state |
|---|---|
| `beepback.h` | done — all types, tunables, function declarations |
| `beepback_rules.c` | done — the seven rules, the windows, seeded generator |
| `test/test_rules.c` | done — 28 host tests, all passing |
| `test/run_tests.sh` | done — compiles with `-Werror` and runs the tests |
| `test/stubs/` | done — fake Flipper headers so tests build on a host |
| `application.fam` | needs its version and source list updated |
| `beepback_led.c` | from the old v3 build, reusable as is |
| `beepback_tables.h` | pre-generated shape outlines and a sine table |

Build the firmware with `ufbt`. Run `./test/run_tests.sh` for the logic, which
needs no device.

## Hard rules

1. **No floats in gameplay code.** There is no multiplier left to round, and
   nothing else in the game needs a fraction. Keep it that way.
2. **Don't touch the generator.** `bb_rng_below` uses a multiply-and-shift
   because a modulo produces a different sequence from the browser, which would
   make the daily a different run on each device. There's a test pinning twelve
   values; keep it passing.
3. **Every screen is 128x64, 1-bit.** Nothing may be drawn outside it. The
   browser build had two bugs from arrows drawn past the bottom edge. The
   fake canvas measures the secondary font at 6px a capital and 9px a line,
   which is what the device draws - it was counting 5 and passing layouts
   whose columns touched.
4. **BACK is the only way back.** LEFT means whatever a screen shows it
   means - a page, a value, a rule - and nothing at all where a screen
   shows nothing. An exit nothing points at is one you find by accident.
5. **`-Werror` stays on.**
6. Add tests as you go. The browser build has 324 checks across sixteen suites
   and they caught roughly a dozen real bugs, several of which were invisible
   by eye.

## The game

Five buttons: UP, DOWN, LEFT, RIGHT, OK. Each has a tone, a colour and a shape,
all three ordered low to high:

| button | tone | colour | shape |
|---|---|---|---|
| DOWN | A4 440Hz | red | circle |
| LEFT | D5 587Hz | yellow | triangle |
| OK | G5 784Hz | green | square |
| RIGHT | C6 1047Hz | blue | pentagon |
| UP | F6 1397Hz | violet | star |

**Assist modes** decide what the player is told: `EARS` (sound only), `LED`,
`SHAPES`, `ARROWS`. Each keeps a separate high score. If volume is 0 and assist
is EARS, force SHAPES — otherwise there is no way to play.

**Modes**

- `CLASSIC` — round *r* builds a sequence to length `3+r`, one step at a time.
  Clearing a stage pays `10 * length`; clearing a round pays `50 * round`.
- `RULES` — same ladder, plus one rule per round that transforms what you press.
- `REFLEX` — no memory. One cue, hit it before the bar empties. Window starts at
  1000ms for everyone and tightens by `bb_rx_shrink[diff]` per hit. A hit pays
  `bb_rx_hit_value(window)`. One miss ends the run, including pressing during
  the gap between cues. **No pausing** — freezing a live cue is a cheat, so BACK
  ends the run instead. Say so on screen before the first cue.
- `CHALLENGE` — pick one rule (or RANDOM) and hold it for the whole run. One
  sequence that grows forever, no rounds, no resets. When growing, re-roll the
  new step if `bb_rule_fits()` fails, so it can never become unpressable.
- `DAILY` — challenge seeded from `bb_today_seed()`. The rule, the sequence and
  the settings all come from the date; TIME and SPEED lock to NORMAL. One
  attempt per day, tracked with the date so it resets at local midnight.

**Three lives** in every mode except reflex, which has one. A mistake replays
the same sequence at the same stage.

## Navigation map

```
launcher (one line: "press a button to play")
  -> splash (brain, dither, ferris wheel, flash, wipe; skippable)
    -> menu, or the tutorial on first run

menu: PLAY / HOW TO PLAY / SETTINGS
  RIGHT -> board picker -> a board -> LEFT back
  board picker RIGHT -> credits
  BACK -> launcher (this is where the save is written)

PLAY -> mode select, three pages of two:
   page 1 CLASSIC RULES | page 2 REFLEX CHALLENGE | page 3 DAILY
   UP/DOWN within a page, LEFT/RIGHT between pages
  CLASSIC/RULES/REFLEX -> setup (TIME, SPEED, START)
  CHALLENGE -> rule picker (7 rules + RANDOM, scrolling) -> setup
  DAILY -> setup showing TODAY/TIME/SPEED as plain rows, cursor pinned to START

HOW TO PLAY -> chooser (CLASSIC / RULES / REFLEX) -> pages -> sound test

SETTINGS: VOLUME / ASSIST / HAPTIC / SOUNDS > / RESET >
  HAPTIC pulses the motor once per note of every pattern, for the note's
  own length less BB_BUZZ_GAP, capped at BB_BUZZ_MAX. Rests get nothing.
  The tune runs whether or not it is audible, so a pattern reaches the
  hand in full with the sound off - bb_play() sets tone_hz to 0 rather
  than dropping the tune.
  RESET -> RECORDS / STATS / TUTORIAL, then EVERYTHING set apart below
    every row asks first: one OK arms it and says SURE?, a second OK
    within BB_CONFIRM_MS does it, moving off the row disarms it
    EVERYTHING is the only one that closes the app
A row of four screens, walked with LEFT and RIGHT, with a chevron at
each end that has somewhere to go:
  MENU <-> STATS <-> RECORDS <-> CREDITS
  bb_chain_step() is the one table; the arrows are drawn from it too, so
  an arrow can never point at a move that does not happen
  STATS: BB_STAT_ROWS rows, five visible, scrolled with UP and DOWN
  RECORDS: pick a mode, OK opens that mode's table
    classic, rules, reflex: TIME down, SPEED across, all twelve at once
    challenge: the seven rules, scrolled with UP and DOWN
    daily: one number per assist, all four at once
    OK cycles the assist, and the title bar says so: the mode is
    left-aligned and "OK: ARR" sits beside it

in game: BACK pauses (reflex: ends the run). OK resumes, BACK quits.
game over: input locked 800ms while the screen wipes down from the top.
```

## Layout constants

- `ROW_L = 14`, `ROW_R = 114` for label and value in list rows
- adjustable rows put their arrows at the row edges, `ROW_AL = 4`,
  `ROW_AR = 118`, value right-aligned at `112`
- **arrows only appear where you can actually go.** No wrapping anywhere: every
  list and every value clamps at its ends, and the arrow disappears there
- title bar: filled black, `y = 0..12`, text centred at 6
- footer strip: filled black, `y = 53..63`, text centred at 58
- body content centres on `y = 32` with a footer, `y = 38` without
- step dots are 5x5 pixel art, not circles. A filled one is a diamond, an empty
  one an outlined square. **Do not draw them with arcs** — in the browser that
  produced different shapes for light and dark ink
- past 14 steps the dots become a plain count, with no total, since the total
  is already in the HUD

## Scoring

The score is a count of what you got right, and nothing else:

- **ten a note.** `BB_NOTE_POINTS`. Clearing a stage pays ten per press in that
  stage, so a stage of six pays 60.
- **a hundred for a round.** `BB_ROUND_BONUS`, ten notes' worth. The stage that
  clears the round pays the bonus instead of a stage award, not as well as one.
- **ten a reflex hit.** A hit pays what a note pays.

There is no multiplier. TIME and SPEED change how hard the run is, not what it
pays, so the number on screen is always one the player could have counted. A
first round comes out at 160 on every setting; a good classic run lands in the
low thousands.

This replaced a system of twelve multipliers in ten-thousandths, a 10x stage
award and a 50x round bonus. Both builds have to agree, so the browser build
needs the same change: drop `MULT_TIME`, `MULT_TIME_RX`, `MULT_SPEED` and the
`score x1.55` footer, pay `10 * expected.length` on success and 100 on round
clear, and pay 10 a hit in reflex.

Records:

- classic, rules, reflex: `best[mode][time][speed][assist]`, shown as a grid
- challenge: `chBest[rule][assist]`, so you can see which rules you're good at
- daily: `daily.best[assist]` plus the date and a done flag

**NEW BEST is judged against the slot you played** (`bb_slot_cell`), never
against a maximum across settings: measuring that way meant a strong INSANE run
lost to an old EASY one and the banner almost never appeared.

`BbStats` is what you have done rather than how well - playtime, runs, notes,
rounds, the run counts behind the favourites - and is saved and reset
separately from the records on purpose.

## What's left, in order

1. **State machine and input.** Scene enum, a tick at `BB_TICK_MS`, the
   input queue. Test: every scene reachable, BACK from each goes where the map
   says, nothing wraps.
2. **Playback and the game loop.** Listen, playback, GO, input, hold, success,
   round clear, wrong, retry, game over. Test: the retry rule replays the same
   sequence, a round clear generates a longer one, scores match the formulas.
3. **The challenge loop.** Endless growth with the re-roll guard. Test: grow 300
   steps, never unpressable, the rule stays fixed, no round ever clears.
4. **Reflex.** Beat-based cues, the shrinking window, one miss ends it, early
   presses count as a miss. Test: the gap matches speed, the window matches the
   ramp, back ends the run.
5. **Screens.** Menus, setup, boards, tutorial, sound test, pause, game over.
   Check every string fits 128px before you commit to it.
6. **Save file.** `/ext/apps_data/beepback/beepback.save`. Magic, version,
   settings, all three record stores, the daily date and flag, then the stats
   block. Bump the version so an old file is discarded rather than misread.
7. **The intro.** Port last. It's the most code for the least behaviour, and
   it's skippable, so nothing else depends on it.

## Things that went wrong in the browser build

Worth knowing, because the same traps are here.

- Constants declared below the code that used them crashed the whole build
  three times. In C that's a compile error, so you'll find out faster.
- A rule guard read `if(mode != RULES) return unchanged`, which silently made
  challenge ignore its own rule. The rule card announced it and the game didn't
  apply it. Test that what you press differs from what was played.
- The HUD showed `stage/target` in a mode that has no target, printing "6/4".
  Challenge shows `LEN n` instead.
- Arrows drawn at `y = 62` ran off the bottom. Anything 9px tall centred below
  y=58 is off-screen.
- The score display showed the unmultiplied award while adding the multiplied
  one, so `+40` went in as `+76`. Both are gone now: there is no multiplier.
