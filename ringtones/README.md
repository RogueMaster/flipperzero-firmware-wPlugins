# ZeroMesh RTTTL Ringtone Files

These are the built-in ringtones written out in RTTTL, plus the format notes
you need to write your own.

## Using your own

ZeroMesh 3.1 and newer reads `.rtttl` files from the SD card. Put them in:

```
/ext/apps_data/zeromesh/ringtones/
```

They appear in Settings after the built-in tones, listed by filename without
the extension. Up to 8 are picked up.

The setup script will copy one across for you:

```
python tools/zeromesh_setup.py --ringtone mytone.rtttl
```

Or compose one in the browser at https://terminalbay.com/?m=ringtone and
download it.

The files in this directory are the built-ins. You do not need to copy them,
they are already in the app, but they are useful as starting points.

## Format

`name:settings:notes`

Settings:
- d = default note length (1, 2, 4, 8, 16, 32)
- o = default octave (3 to 8)
- b = tempo in BPM

Notes, comma separated:
- `[length][note][#][octave][.]`
- note is a to g, or p for a rest
- `#` makes it sharp, a trailing `.` makes it half again as long
- length and octave both fall back to the defaults

`8c6` is an eighth note, C, octave 6.

Example: `Custom:d=8,o=5,b=160:g#,p,c6`

## Limits

A tone stops after 12 seconds or 192 notes, whichever comes first. Keep
notification tones under about two seconds.

The Flipper speaker is a buzzer, so it plays one note at a time. Short and
distinctive beats long and musical. Frequencies between 300 Hz and 1200 Hz
carry best.
