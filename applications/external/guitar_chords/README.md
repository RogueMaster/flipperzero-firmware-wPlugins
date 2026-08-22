# Guitar Chords — Flipper Zero app

Browse guitar chord diagrams on the Flipper. Root note → chord → diagram, with
multiple voicings per chord. The library lives on the SD card as a plain text
file, so adding chords doesn't require rebuilding.

```
guitar_chords/
├── application.fam      manifest (appid, entry point, category)
├── guitar_chords.c      app lifecycle, ViewDispatcher, splash, menus, navigation
├── chord_db.h/.c        chord struct, CSV + builtin loading, queries
├── chord_view.h/.c      the diagram View (drawing + Left/Right/Up/Down input)
├── song_db.h/.c         progression struct, songs.csv + builtin loading
├── practice_view.h/.c   the practice View (metronome display + input)
├── icon.png             10x10 1-bit menu icon
└── assets/chords.csv    library file to copy to the SD card
```

A splash screen credits **KingBoa / NoodleNugget.com** on launch and stays up
until you press a key.

## Build

```bash
pipx install ufbt   # or: pip install --upgrade ufbt
cd guitar_chords
ufbt              # build only -> dist/guitar_chords.fap
ufbt launch       # build, upload, and run on a connected Flipper
ufbt cli          # serial console, useful for FURI_LOG output
```

**Run ufbt from inside `guitar_chords/`, not the parent.** ufbt treats the
current directory as the app folder and never searches for the manifest. From
one level up you get `missing manifest (application.fam)`, and `ufbt launch`
reports the resulting empty app list as the confusing
`More than one app is runnable: .`

Verified against the official **stock `release` channel, SDK 1.4.3** (target f7,
API 87.1). Builds clean under `-Wall -Wextra -Werror`.

First run downloads the SDK for your firmware channel. For a non-stock firmware:

```bash
ufbt update --channel=dev
# Momentum / Unleashed / RogueMaster: ufbt update --index-url=<their index URL>
```

The built `.fap` also works if you just drop it in `SD:/apps/Tools/`.

## Chord library

On first launch the app writes a starter library to:

```
SD:/apps_data/guitar_chords/chords.csv
```

Edit that file (qFlipper's file manager, or pull the SD card) and restart the app.
If the file is missing or unparseable, the compiled-in set is used instead.

> **The seed only happens when the file is absent.** A rebuilt `.fap` will *not*
> overwrite a `chords.csv` that already exists, so new builtin chords won't show
> up until you delete it and relaunch:
>
> ```
> $ ufbt cli                                               <- in your shell
> >: storage remove /ext/apps_data/guitar_chords/chords.csv <- at the Flipper prompt
> >: storage remove /ext/apps_data/guitar_chords/songs.csv
> >: exit
> ```
>
> `storage` is a Flipper command, not a shell one — it only works at the `>:`
> prompt that `ufbt cli` opens. Copying the file over in qFlipper does the same
> job with less ceremony.

Format, one chord per line:

```
NAME|FRETS|BASE_FRET|FINGERS
```

| Field | Meaning |
|---|---|
| `NAME` | Displayed name. The leading letter plus optional `#`/`b` becomes the root, so `Am7` files under **A**. |
| `FRETS` | Exactly 6 characters, **low E first**. `x` = muted, `0` = open, `1`–`9` = frets above `BASE_FRET`. |
| `BASE_FRET` | Fret the diagram starts on. `1` draws the nut. Optional, defaults to 1. |
| `FINGERS` | Exactly 6 characters, `-` = none, `1`–`4` = finger. Optional. |

Examples:

```
Am|x02210|1|--231-
Bm|x13321|2|-13421      <- A-minor shape barred at fret 2
Eb|x13331|6|-13421      <- A-major shape barred at fret 6
```

Mind the 4th digit: the A-**minor** barre shape is `x13321`, the A-**major**
one is `x13331`. Mixing them up gives you a major chord under a minor name.

Repeating the same finger across adjacent strings on one fret draws a barre bar
instead of separate dots. If you write absolute frets by mistake (`x-6-8-8-8-6`
with base 1), the parser slides the window for you.

### Multiple voicings

**Reusing a NAME adds a voicing rather than replacing one.** Every line sharing a
name becomes another way to play that chord, cycled with Left/Right in the
diagram view, in the order the lines appear in the file — so keep the open
position first:

```
C|x32010|1|-32-1-     <- open, shown first
C|x13331|3|-13421     <- A-shape barre at 3
C|133211|8|134211     <- E-shape barre at 8
```

The diagram shows `1/3` in the top-right and `<` `>` hints at the edges only
when a chord has more than one voicing. The shipped library has 71 entries
covering 35 chord names; 27 of them carry 2–3 voicings.

Frets must land within the 4-fret display window (offsets `1`–`4`). A dot at
offset 5 or beyond is silently not drawn — raise `FRET_ROWS` or pick a higher
`BASE_FRET` instead.

## Practice mode

**Practice** sits at the top of the root menu. Pick a progression and it becomes
a metronome that shows you the chord to play. Each chord lasts one bar of four
beats: a click on every beat, and a higher click plus a vibro bump on the
downbeat when the chord changes, so you can keep your eyes on your hands.

It starts **paused** — press OK to begin.

Progressions live in a second SD file, seeded the same way as the chord library:

```
SD:/apps_data/guitar_chords/songs.csv
```

```
NAME|CHORDS|BPM
```

| Field | Meaning |
|---|---|
| `NAME` | Shown in the progression menu. Up to 21 characters. |
| `CHORDS` | 2–8 chord names separated by commas. Each must match a `NAME` in `chords.csv`. |
| `BPM` | 40–200. Optional, defaults to 80. One chord per bar, so 80 bpm holds each chord for 3 seconds. |

```
I-V-vi-IV|G,D,Em,C|85
ii-V-I|Dm7,G7,Cmaj7|75
12-Bar Blues|A7,D7,E7|90
```

14 progressions ship by default — five 2-chord drills, four 3-chord, five
4-chord. A chord name that isn't in the library shows `not in library` for that
step rather than failing to load, so a typo costs you one step, not the file.

Practice always uses each chord's **first** voicing, which is the open position
where one exists. Reorder the lines in `chords.csv` to change which shape it
picks.

## Controls

Splash screen:

| | |
|---|---|
| any key | continue to the root menu |
| Back | exit the app |

Menus:

| | |
|---|---|
| Up / Down | move through the menu |
| OK | select |
| Back | up one level; exits from the root menu |

Diagram view:

| | |
|---|---|
| Left / Right | previous / next **voicing** of this chord |
| Up / Down | previous / next **chord** in the root's list |
| Back | back to the chord menu |

Practice view:

| | |
|---|---|
| OK | play / pause |
| Up / Down | tempo +/- 5 bpm (40–200) |
| Left / Right | step back / forward one chord, and pause |
| Back | back to the progression menu (stops the metronome) |

Both Left/Right and Up/Down wrap around, and both accept held-repeat for fast
scrolling. Stepping to a new chord resets to its first voicing, since voicing
counts differ per chord. The chord menu's highlight follows Up/Down, so Back
returns you to the chord you were actually looking at.

## Notes / gotchas

- The screen is 128×64 monochrome. The diagram uses a fixed 4-fret window
  (`FRET_ROWS` in `chord_view.c`) — widen it there if you want 5. Note that
  `draw_dots()` skips anything outside that window without warning.
- `application.fam` sets `sources=["*.c"]`. Do not drop back to fbt's default
  `["*.c*"]`: that glob also matches `assets/chords.csv` and hands it to the
  linker, which fails with `file format not recognized`.
- On SDK 1.4.3 `view_dispatcher_enable_queue()` still exists but is
  `FURI_DEPRECATED`, so the app does not call it. Firmware older than 0.98
  requires it right after `view_dispatcher_alloc()` — that's the line to add if
  you build for something ancient.
- `stack_size` is 4 KB in the manifest. If you load a very large CSV and hit a
  crash on launch, raise it. The chord array lives on the heap (doubling from 32
  entries), not the stack, so the 71-entry library costs ~6 KB of heap.
- The icon is `icon.png`, a 10×10 1-bit PNG referenced by `fap_icon`. fbt
  rejects any other size or bit depth.

## Worth building next

- **Transpose / capo mode** — hold a capo offset in app state and shift
  `base_fret` on display. Given a capo on 3, a `C` shape would label itself `Eb`.
- **Play the chord** — the Flipper's speaker is a single square-wave channel, so
  it can't strum, but it can arpeggiate. `furi_hal_speaker_start(freq, volume)`
  in a short loop over the six string pitches gives you a usable reference.
- **Progression mode** — a second file format (`songs.csv`) listing chord names
  per song, then Left/Right steps through the progression instead of voicings.
