# Theme Player for Flipper Zero

Pick a theme, pick a tune, and it loops in 8-bit style on the Flipper speaker.

## Controls
- **Themes / tunes list:** Up/Down to move, OK to open/play, Back to go up
- **Playing:** Left/Right previous/next tune, Up/Down volume, OK toggles loop, Back stops

## Built-in themes
Wizard, Adventure, Space, Spooky, Retro Arcade. All built-in tunes are original
compositions written for this app.

## Adding your own tunes
Make a folder per theme on the SD card and drop tune files in it:

```
SD:/apps_data/theme_player/<Theme Name>/<tune>.rtttl
```

Supported formats:
- RTTTL ringtone text (`.rtttl` or `.txt`), e.g. `Name:d=4,o=5,b=120:c,e,g,2c6`
- Flipper Music Format (`.fmf`, as used by the stock Music Player)

A folder whose name matches a built-in theme adds its tunes to that theme.
Tune files you add stay on your SD card; they are not part of this repo.

## Converting a MIDI file
`scripts/midi_to_rtttl.py` turns a `.mid` file into an 8-bit RTTTL tune (melody only, one note at a time):

```
python scripts/midi_to_rtttl.py song.mid --list
python scripts/midi_to_rtttl.py song.mid --track 1 --name "My Tune"
```

Options: `--track N` picks the melody track (default: the busiest one), `--transpose N`
shifts by semitones, `--out file.rtttl` sets the output path. Copy the result into a theme folder.

## Build
```
ufbt
```
