#include "intervals.h"

#include <furi.h>

/* Tables live in flash (static const), not RAM. */
static const IntervalInfo intervals[IntervalCount] = {
    {"P1", "Unison", "the same note twice"},
    {"m2", "Minor 2nd", "Jaws"},
    {"M2", "Major 2nd", "Happy Birthday"},
    {"m3", "Minor 3rd", "Greensleeves"},
    {"M3", "Major 3rd", "When the Saints"},
    {"P4", "Perfect 4th", "Here Comes the Bride"},
    {"TT", "Tritone", "The Simpsons"},
    {"P5", "Perfect 5th", "Star Wars theme"},
    {"m6", "Minor 6th", "The Entertainer"},
    {"M6", "Major 6th", "My Bonnie"},
    {"m7", "Minor 7th", "Star Trek theme"},
    {"M7", "Major 7th", "Take On Me"},
    {"P8", "Octave", "Over the Rainbow"},
};

const IntervalInfo* interval_get(uint8_t semitones) {
    if(semitones >= IntervalCount) return &intervals[0];
    return &intervals[semitones];
}

/* Equal temperament from C4 up to C6, tenths of a hertz stored as integers so
 * the table costs no float relocations and needs no powf at runtime. */
static const uint16_t note_freq_tenths[NOTE_MIDI_MAX - NOTE_MIDI_MIN + 1] = {
    2616, /* C4  */
    2772, /* C#4 */
    2937, /* D4  */
    3111, /* D#4 */
    3296, /* E4  */
    3492, /* F4  */
    3700, /* F#4 */
    3920, /* G4  */
    4153, /* G#4 */
    4400, /* A4  */
    4662, /* A#4 */
    4939, /* B4  */
    5233, /* C5  */
    5544, /* C#5 */
    5873, /* D5  */
    6223, /* D#5 */
    6593, /* E5  */
    6985, /* F5  */
    7400, /* F#5 */
    7840, /* G5  */
    8306, /* G#5 */
    8800, /* A5  */
    9323, /* A#5 */
    9878, /* B5  */
    10465, /* C6  */
};

float note_frequency(uint8_t midi_note) {
    if(midi_note < NOTE_MIDI_MIN || midi_note > NOTE_MIDI_MAX) return 0.0f;
    return note_freq_tenths[midi_note - NOTE_MIDI_MIN] / 10.0f;
}

void note_name(uint8_t midi_note, char* buf, uint8_t buf_size) {
    static const char* const names[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    /* MIDI 60 is C4, and 12 notes per octave puts C-1 at 0. */
    snprintf(buf, buf_size, "%s%d", names[midi_note % 12], (midi_note / 12) - 1);
}
