#include "chords.h"

/* Tables live in flash (static const), not RAM. */

static const Pattern chords[ChordCount] = {
    {"maj", "Major", "bright, resolved", 3, {0, 4, 7}},
    {"min", "Minor", "darker, sadder", 3, {0, 3, 7}},
    {"dim", "Diminished", "tense, unstable", 3, {0, 3, 6}},
    {"aug", "Augmented", "dreamlike, floating", 3, {0, 4, 8}},
    {"sus4", "Suspended 4th", "unresolved, hanging", 3, {0, 5, 7}},
    {"maj7", "Major 7th", "lush, jazzy", 4, {0, 4, 7, 11}},
    {"min7", "Minor 7th", "smooth, mellow", 4, {0, 3, 7, 10}},
    {"dom7", "Dominant 7th", "bluesy, wants to move", 4, {0, 4, 7, 10}},
    {"m7b5", "Half-diminished", "moody, jazz minor", 4, {0, 3, 6, 10}},
};

/* Scales run root to octave so the ear hears the whole shape close. */
static const Pattern scales[ScaleCount] = {
    {"Maj", "Major", "do re mi, happy", 8, {0, 2, 4, 5, 7, 9, 11, 12}},
    {"min", "Natural minor", "sad, the relative minor", 8, {0, 2, 3, 5, 7, 8, 10, 12}},
    {"hmin", "Harmonic minor", "exotic jump near the top", 8, {0, 2, 3, 5, 7, 8, 11, 12}},
    {"Dor", "Dorian", "minor but hopeful", 8, {0, 2, 3, 5, 7, 9, 10, 12}},
    {"Mix", "Mixolydian", "major with a flat 7", 8, {0, 2, 4, 5, 7, 9, 10, 12}},
    {"Pent", "Major pentatonic", "five notes, folk", 6, {0, 2, 4, 7, 9, 12}},
    {"mPent", "Minor pentatonic", "five notes, rock solos", 6, {0, 3, 5, 7, 10, 12}},
    {"Blues", "Blues", "pentatonic plus the blue note", 7, {0, 3, 5, 6, 7, 10, 12}},
    {"Whole", "Whole tone", "every step the same, eerie", 7, {0, 2, 4, 6, 8, 10, 12}},
};

const Pattern* chord_get(uint8_t id) {
    if(id >= ChordCount) id = 0;
    return &chords[id];
}

const Pattern* scale_get(uint8_t id) {
    if(id >= ScaleCount) id = 0;
    return &scales[id];
}

uint8_t pattern_span(const Pattern* pattern) {
    uint8_t max = 0;
    for(uint8_t i = 0; i < pattern->count; i++) {
        if(pattern->steps[i] > max) max = pattern->steps[i];
    }
    return max;
}
