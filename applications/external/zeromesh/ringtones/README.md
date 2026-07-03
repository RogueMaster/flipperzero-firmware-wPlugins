# ZeroMesh RTTTL Ringtone Files

These RTTTL files correspond to the built-in ringtones in ZeroMesh.

## Usage

These files are for reference and testing. All ringtones are built directly into the ZeroMesh firmware, so you don't need to copy these files to your Flipper Zero.

## Testing RTTTL Files

You can test these files using:
- Online RTTTL players: https://adamonsoon.github.io/rtttl-play/
- Arduino RTTTL libraries
- Other RTTTL-compatible players

## File Format

RTTTL format: `name:settings:notes`

Settings:
- d = default duration (1,2,4,8,16,32)
- o = default octave (4-7)
- b = tempo in BPM

Notes:
- Format: duration+note+octave
- p = pause/rest
- # = sharp

Example: `8c6` = eighth note, C, octave 6

## Creating Custom Ringtones

1. Compose your melody in RTTTL format
2. Keep it under 2 seconds for notifications
3. Test with an online player
4. Add the code to zeromesh_notify.c
5. Update zeromesh_serial.h enum
6. Update ringtone_names[] array
7. Recompile firmware

All ringtones should use frequencies between 300Hz - 1200Hz for best Flipper Zero speaker performance.
