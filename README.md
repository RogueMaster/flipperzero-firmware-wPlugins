# Ear Trainer

Learn to recognise intervals, chords and scales by ear, on your Flipper Zero.

I kept trying to pick out melodies and kept guessing at the jumps between
notes. This is the drill I wanted: it plays you something and you name it. You
start with the things nobody mixes up — an octave, a fifth, major against
minor — and the close, nasty ones arrive only once the easy ones are automatic.

## What you can train

**Intervals** — two notes, name the gap. Three modes, each with its own saved
progress, because hearing a descending minor 6th is genuinely a different
skill from hearing it ascending:

- Ascending, the low note first
- Descending, the high note first
- Mixed, either direction at random

**Chords** — major, minor, diminished, augmented, sus4, then the 7ths
(major 7th, minor 7th, dominant 7th, half-diminished). Played as arpeggios,
one note after another, because the Flipper's speaker can only hold one
frequency at a time.

**Scales** — major, natural minor, harmonic minor, Dorian, Mixolydian, both
pentatonics, blues and whole tone. Played root to octave so you hear the whole
shape at once.

## Levels

Thirteen levels for intervals, eight each for chords and scales, always widest
contrast first. Interval levels run unison and octave, then the fifth and
fourth, on through the thirds, sixths and sevenths, with the tritone last.
Chords open on major against minor. Scales open on major against natural
minor.

Levels that add something new introduce it first: the name, a keyboard showing
the shape, and a line on what it sounds like, with a play button so you can
hear it as often as you like before the quiz starts. Mix levels add nothing new
and instead throw everything learned so far at you, with three lives.

Clearing a level unlocks the next. Stars are 80 percent, 90 percent, perfect.

## Controls

In a quiz:

| Key | Action |
|-----|--------|
| Left / Right | Move through the answers |
| OK | Confirm |
| Up | Replay |
| Down | Spend a hint — strikes out wrong answers |
| Back | Press twice to leave |

Two hints per quiz, and a hint never strikes out so many that the answer is the
only thing left. Back needs two presses so a stray thumb doesn't bin a run.

## The keyboard

After you answer, and on every teaching screen, a two-octave keyboard lights up
the notes that were actually played. Hearing a minor 7th is one thing; seeing
the gap on a keyboard is what makes it stick.

## Hints

Every interval carries a tune that opens with it, which is the fastest way to
get from "I hear a gap" to "that's a fourth":

| | | | |
|---|---|---|---|
| m2 | Jaws | m6 | The Entertainer |
| M2 | Happy Birthday | M6 | My Bonnie |
| m3 | Greensleeves | m7 | Star Trek theme |
| M3 | When the Saints | M7 | Take On Me |
| P4 | Here Comes the Bride | P8 | Over the Rainbow |
| TT | The Simpsons | | |
| P5 | Star Wars theme | | |

Chords and scales carry a character note instead — augmented is "dreamlike,
floating", harmonic minor is "exotic jump near the top".

There is also an interval reference screen in the menu that plays any of the
thirteen on demand, handy before a session or to re-anchor after a bad run.

## Settings

- Note length, short to long
- Root note, fixed C4 or random each question. Random is the default and it
  matters: a fixed root lets you memorise pitches instead of learning the
  distance between them, which is the whole point.
- Tune and character hints on the answer screen
- Vibro on a miss, LED feedback

Progress and settings live in apps_data/ear_trainer on the SD card and survive
reboots.

## Notes on the build

Tones come out of the piezo on a dedicated worker thread with its own message
queue, so playback never blocks the UI and a replay can cut off whatever is
already sounding. Note frequencies are a fixed-point table from C4 to C6 rather
than computing powers at runtime; the table was checked against equal
temperament and every note lands within 0.1 cents.

The saved files carry a magic number and a version. Adding chords and scales
grew the save from three modes to five, which changed the shape of the stars
array, so version 1 files are migrated field by field rather than read as a
byte prefix — old progress and lifetime stats carry over intact. Anything
unexpected resets to defaults instead of crashing.

## Building

Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt), then from the
project root run ufbt update --channel=release followed by ufbt.

Run ufbt launch instead to upload and start it on a Flipper over USB. The built
app lands in the dist folder (SD card folder: apps/Media).

## License

MIT — see [LICENSE](LICENSE).
