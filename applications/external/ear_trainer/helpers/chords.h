#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Chords and scales are both just a list of semitone offsets from a root, so
 * they share one shape. A chord is played as an arpeggio: the speaker is
 * monophonic, so notes sound one after another rather than together. */

#define MAX_PATTERN_NOTES 9

typedef struct {
    const char* shortname; /* fits the answer grid, e.g. "maj7" */
    const char* name; /* "Major 7th" */
    const char* hint; /* what it sounds/feels like */
    uint8_t count;
    uint8_t steps[MAX_PATTERN_NOTES]; /* semitones from the root */
} Pattern;

typedef enum {
    ChordMajor,
    ChordMinor,
    ChordDiminished,
    ChordAugmented,
    ChordSus4,
    ChordMajor7,
    ChordMinor7,
    ChordDominant7,
    ChordHalfDim7,
    ChordCount,
} ChordId;

typedef enum {
    ScaleMajor,
    ScaleNaturalMinor,
    ScaleHarmonicMinor,
    ScaleDorian,
    ScaleMixolydian,
    ScaleMajorPentatonic,
    ScaleMinorPentatonic,
    ScaleBlues,
    ScaleWholeTone,
    ScaleCount,
} ScaleId;

const Pattern* chord_get(uint8_t id);
const Pattern* scale_get(uint8_t id);

/* Widest span in semitones, so a root can be chosen that keeps every note
 * inside the playable note table. */
uint8_t pattern_span(const Pattern* pattern);
