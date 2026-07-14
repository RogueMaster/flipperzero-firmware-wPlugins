# MIDI Player

MIDI Player is a Flipper Zero external app that plays Standard MIDI files as retro buzzer music through the built-in speaker.

The Flipper speaker is a simple tone buzzer, so this is not a full General MIDI synth or soundfont player. MIDI Player converts MIDI notes into monophonic buzzer playback and uses lightweight playback rules to make normal MIDI files degrade as cleanly as possible.

## Features

- File picker for `.mid` files on the SD card.
- Standard MIDI format 0 and format 1 parsing.
- Best-effort format 2 playback.
- PPQ timing and basic SMPTE timing support.
- Tempo changes.
- Running status.
- Note on/off, including velocity-zero note-off.
- Program change handling for simple buzzer styles.
- Smart monophonic note selection, with drum channel ignored by default.
- Pause/resume and master volume controls.
- Parser coverage for malformed files and common MIDI edge cases.

## SD Card Layout

Copy MIDI files here:

```text
/ext/apps_data/midi_player/songs/
```

For example:

```text
/ext/apps_data/midi_player/songs/example.mid
```

The app creates the `midi_player` and `songs` folders when it opens the file picker, but it does not download or generate songs on the device.

## Controls

- `OK`: pause or resume playback.
- `Up`: increase volume.
- `Down`: decrease volume.
- `Back`: stop playback and exit.

## Building

From this directory:

```sh
ufbt
```

The built app is written to:

```text
dist/midi_player.fap
```

Copy it to the Flipper SD card under:

```text
/ext/apps/Media/
```

## Limitations

- Playback is intentionally monophonic because the built-in speaker can only produce one buzzer tone at a time.
- Instruments are approximated with simple frequency, envelope, and volume changes.
- Some dense MIDI files will sound simplified because only one note can be audible at once.
- Full soundfont synthesis, external MIDI output, visualizers, and full General MIDI accuracy are out of scope for this release.

## License

MIT License. See `LICENSE`.
