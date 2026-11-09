"""Convert a MIDI file into an 8-bit RTTTL tune for Theme Player.

Usage:
    python midi_to_rtttl.py song.mid [--track N] [--name "Title"] [--out song.rtttl]
                            [--transpose SEMITONES] [--list]

The Flipper speaker plays one note at a time, so the highest sounding note
(usually the melody) of the chosen track is kept. No third-party packages.
"""

import argparse
import struct
import sys
from pathlib import Path

NAMES = ["c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"]
# (beats, rtttl duration, dotted)
DURATIONS = sorted(
    [
        (4 / d * (1.5 if dot else 1), d, dot)
        for d in (1, 2, 4, 8, 16, 32)
        for dot in (False, True)
    ],
    reverse=True,
)


def read_var(data, i):
    value = 0
    while True:
        b = data[i]
        i += 1
        value = (value << 7) | (b & 0x7F)
        if not b & 0x80:
            return value, i


def parse(path):
    data = Path(path).read_bytes()
    if data[:4] != b"MThd":
        raise ValueError("Not a MIDI file")
    _, _, ntracks, division = struct.unpack(">IHHH", data[4:14])
    if division & 0x8000:
        raise ValueError("SMPTE-timed MIDI files are not supported")
    i, tracks, tempo = 8 + struct.unpack(">I", data[4:8])[0], [], 500000
    for _ in range(ntracks):
        if data[i : i + 4] != b"MTrk":
            break
        length = struct.unpack(">I", data[i + 4 : i + 8])[0]
        j, end, tick, status = i + 8, i + 8 + length, 0, 0
        notes, active, name = [], {}, ""
        while j < end:
            delta, j = read_var(data, j)
            tick += delta
            if data[j] & 0x80:
                status = data[j]
                j += 1
            kind = status & 0xF0
            if status == 0xFF:
                meta = data[j]
                size, j = read_var(data, j + 1)
                body = data[j : j + size]
                if meta == 0x51 and tempo == 500000:
                    tempo = int.from_bytes(body, "big")
                elif meta == 0x03:
                    name = body.decode("latin-1", "replace")
                j += size
            elif status in (0xF0, 0xF7):
                size, j = read_var(data, j)
                j += size
            elif kind in (0x80, 0x90):
                note, vel = data[j], data[j + 1]
                j += 2
                if kind == 0x90 and vel:
                    active[note] = tick
                elif note in active:
                    notes.append((active.pop(note), tick, note))
            elif kind in (0xC0, 0xD0):
                j += 1
            else:
                j += 2
        tracks.append((name, sorted(notes)))
        i = end
    return tracks, division, tempo


def melody(notes):
    """Keep the highest note at each moment so chords collapse to a single line."""
    events = []
    for start, end, pitch in notes:
        events.append((start, end, pitch))
    events.sort(key=lambda e: (e[0], -e[2]))
    line = []
    for start, end, pitch in events:
        if line and start < line[-1][1]:
            if pitch <= line[-1][2] and start != line[-1][0]:
                continue
            if start == line[-1][0]:
                continue
            line[-1] = (line[-1][0], start, line[-1][2])
        line.append((start, end, pitch))
    return line


def fit(beats):
    out = []
    while beats > 1 / 16:
        beats_d, d, dot = next(
            ((b, d, dot) for b, d, dot in DURATIONS if b <= beats + 1e-6), DURATIONS[-1]
        )
        out.append((d, dot))
        beats -= beats_d
    return out


def to_rtttl(line, division, tempo, title, transpose):
    bpm = max(25, min(900, round(60_000_000 / tempo)))
    tokens, cursor = [], line[0][0] if line else 0
    for start, end, pitch in line:
        if start > cursor:
            tokens += [
                f"{d}p{'.' if dot else ''}"
                for d, dot in fit((start - cursor) / division)
            ]
        pitch += transpose
        octave = max(4, min(7, pitch // 12 - 1))
        for d, dot in fit((end - start) / division):
            tokens.append(f"{d}{NAMES[pitch % 12]}{'.' if dot else ''}{octave}")
        cursor = end
    return f"{title[:24]}:d=4,o=5,b={bpm}:" + ",".join(tokens)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("midi")
    ap.add_argument("--track", type=int)
    ap.add_argument("--name")
    ap.add_argument("--out")
    ap.add_argument("--transpose", type=int, default=0)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    tracks, division, tempo = parse(args.midi)
    if args.list:
        for i, (name, notes) in enumerate(tracks):
            print(f"{i}: {name or '(unnamed)'} - {len(notes)} notes")
        return 0
    index = (
        args.track
        if args.track is not None
        else max(range(len(tracks)), key=lambda k: len(tracks[k][1]))
    )
    line = melody(tracks[index][1])
    if not line:
        print("That track has no notes; use --list to pick another.", file=sys.stderr)
        return 1
    title = args.name or Path(args.midi).stem
    text = to_rtttl(line, division, tempo, title, args.transpose)
    out = Path(args.out or Path(args.midi).with_suffix(".rtttl"))
    out.write_text(text)
    print(f"Wrote {out} from track {index} ({len(line)} notes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
