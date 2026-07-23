#pragma once

#include <stdbool.h>
#include <stdint.h>

/* The twelve intervals inside an octave, plus the octave itself. Index is the
 * number of semitones, which keeps every lookup a plain array index. */
typedef enum {
    IntervalUnison = 0,
    IntervalMinor2,
    IntervalMajor2,
    IntervalMinor3,
    IntervalMajor3,
    IntervalPerfect4,
    IntervalTritone,
    IntervalPerfect5,
    IntervalMinor6,
    IntervalMajor6,
    IntervalMinor7,
    IntervalMajor7,
    IntervalOctave,
    IntervalCount,
} Interval;

typedef struct {
    const char* shortname; /* "P5" - fits the answer list */
    const char* name; /* "Perfect 5th" */
    const char* mnemonic; /* a tune that opens with it */
} IntervalInfo;

const IntervalInfo* interval_get(uint8_t semitones);

/* Lowest and highest root note the quiz may pick, as MIDI note numbers.
 * The window is chosen so root + an octave still lands inside the table and
 * stays in the range where the piezo is actually audible. */
#define ROOT_MIDI_MIN 60 /* C4  */
#define ROOT_MIDI_MAX 72 /* C5  */
#define NOTE_MIDI_MIN 60
#define NOTE_MIDI_MAX 84 /* C6  */

/* Frequency in Hz for a MIDI note number, or 0.0f if out of range. */
float note_frequency(uint8_t midi_note);

/* "C4", "F#4" ... written into buf. */
void note_name(uint8_t midi_note, char* buf, uint8_t buf_size);
