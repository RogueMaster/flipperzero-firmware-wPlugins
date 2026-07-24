#include "curriculum.h"

/* --- intervals: widest first, because an octave and a fifth are easy to
 * tell apart while seconds and the tritone are not. --- */

static const uint8_t iv_l0[] = {IntervalUnison, IntervalOctave};
static const uint8_t iv_l1[] = {IntervalPerfect5};
static const uint8_t iv_l2[] = {IntervalPerfect4};
static const uint8_t iv_l4[] = {IntervalMajor3};
static const uint8_t iv_l5[] = {IntervalMinor3};
static const uint8_t iv_l7[] = {IntervalMajor2};
static const uint8_t iv_l8[] = {IntervalMajor6};
static const uint8_t iv_l9[] = {IntervalMinor6};
static const uint8_t iv_l10[] = {IntervalMinor2};
static const uint8_t iv_l11[] = {IntervalMinor7, IntervalMajor7};
static const uint8_t iv_l12[] = {IntervalTritone};

/* --- chords: the major/minor contrast first, then the colours, then 7ths --- */

static const uint8_t ch_l0[] = {ChordMajor, ChordMinor};
static const uint8_t ch_l1[] = {ChordDiminished};
static const uint8_t ch_l3[] = {ChordAugmented, ChordSus4};
static const uint8_t ch_l5[] = {ChordMajor7, ChordMinor7};
static const uint8_t ch_l6[] = {ChordDominant7, ChordHalfDim7};

/* --- scales: major vs minor first, then pentatonics, then the modes --- */

static const uint8_t sc_l0[] = {ScaleMajor, ScaleNaturalMinor};
static const uint8_t sc_l2[] = {ScaleMajorPentatonic, ScaleMinorPentatonic};
static const uint8_t sc_l4[] = {ScaleDorian, ScaleMixolydian};
static const uint8_t sc_l5[] = {ScaleHarmonicMinor};
static const uint8_t sc_l6[] = {ScaleBlues, ScaleWholeTone};

#define LEVEL(lbl, arr) {lbl, arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])), false}
#define REVIEW(lbl)     {lbl, NULL, 0, true}

static const EarLevel interval_levels[] = {
    LEVEL("P1 P8", iv_l0),
    LEVEL("P5", iv_l1),
    LEVEL("P4", iv_l2),
    REVIEW("Mix 1"),
    LEVEL("M3", iv_l4),
    LEVEL("m3", iv_l5),
    REVIEW("Mix 2"),
    LEVEL("M2", iv_l7),
    LEVEL("M6", iv_l8),
    LEVEL("m6", iv_l9),
    LEVEL("m2", iv_l10),
    LEVEL("m7 M7", iv_l11),
    LEVEL("TT", iv_l12),
};

static const EarLevel chord_levels[] = {
    LEVEL("maj min", ch_l0),
    LEVEL("dim", ch_l1),
    REVIEW("Mix 1"),
    LEVEL("aug sus4", ch_l3),
    REVIEW("Mix 2"),
    LEVEL("maj7 min7", ch_l5),
    LEVEL("dom7 m7b5", ch_l6),
    REVIEW("Mix 3"),
};

static const EarLevel scale_levels[] = {
    LEVEL("Maj min", sc_l0),
    REVIEW("Mix 1"),
    LEVEL("Pentatonic", sc_l2),
    REVIEW("Mix 2"),
    LEVEL("Dor Mix", sc_l4),
    LEVEL("Harmonic", sc_l5),
    LEVEL("Blues Whole", sc_l6),
    REVIEW("Mix 3"),
};

#define ARRAY_LEN(a) ((uint8_t)(sizeof(a) / sizeof(a[0])))

ContentType mode_content(uint8_t mode) {
    if(mode == ModeChords) return ContentChord;
    if(mode == ModeScales) return ContentScale;
    return ContentInterval;
}

const char* mode_name(uint8_t mode) {
    static const char* const names[MODE_COUNT] = {
        "Ascending",
        "Descending",
        "Mixed",
        "Chords",
        "Scales",
    };
    return names[mode < MODE_COUNT ? mode : 0];
}

static const EarLevel* levels_for(uint8_t mode, uint8_t* count) {
    switch(mode_content(mode)) {
    case ContentChord:
        *count = ARRAY_LEN(chord_levels);
        return chord_levels;
    case ContentScale:
        *count = ARRAY_LEN(scale_levels);
        return scale_levels;
    case ContentInterval:
    default:
        *count = ARRAY_LEN(interval_levels);
        return interval_levels;
    }
}

uint8_t curriculum_level_count(uint8_t mode) {
    uint8_t count;
    levels_for(mode, &count);
    return count;
}

const EarLevel* curriculum_get(uint8_t mode, uint8_t level_index) {
    uint8_t count;
    const EarLevel* levels = levels_for(mode, &count);
    if(level_index >= count) level_index = count - 1;
    return &levels[level_index];
}

bool curriculum_is_challenge(uint8_t mode, uint8_t level_index) {
    return curriculum_get(mode, level_index)->challenge;
}

uint8_t
    curriculum_learned_upto(uint8_t mode, uint8_t level_index, uint8_t* buf, uint8_t buf_size) {
    uint8_t count;
    const EarLevel* levels = levels_for(mode, &count);
    if(level_index >= count) level_index = count - 1;

    /* IntervalCount is the largest id space of the three, so it sizes the
     * seen-set for every content type. */
    bool seen[IntervalCount] = {0};
    for(uint8_t l = 0; l <= level_index; l++) {
        for(uint8_t i = 0; i < levels[l].new_count; i++) {
            uint8_t id = levels[l].new_items[i];
            if(id < IntervalCount) seen[id] = true;
        }
    }

    uint8_t written = 0;
    for(uint8_t id = 0; id < IntervalCount && written < buf_size; id++) {
        if(seen[id]) buf[written++] = id;
    }
    return written;
}

const char* content_shortname(uint8_t mode, uint8_t id) {
    switch(mode_content(mode)) {
    case ContentChord:
        return chord_get(id)->shortname;
    case ContentScale:
        return scale_get(id)->shortname;
    default:
        return interval_get(id)->shortname;
    }
}

const char* content_name(uint8_t mode, uint8_t id) {
    switch(mode_content(mode)) {
    case ContentChord:
        return chord_get(id)->name;
    case ContentScale:
        return scale_get(id)->name;
    default:
        return interval_get(id)->name;
    }
}

const char* content_hint(uint8_t mode, uint8_t id) {
    switch(mode_content(mode)) {
    case ContentChord:
        return chord_get(id)->hint;
    case ContentScale:
        return scale_get(id)->hint;
    default:
        return interval_get(id)->mnemonic;
    }
}
