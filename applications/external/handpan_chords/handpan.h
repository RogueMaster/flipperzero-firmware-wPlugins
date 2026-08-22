#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

/* A handpan has one ding plus a ring of tone fields. All nine drums shipped
 * here have eight tone fields, but the diagram and the chord builder support
 * nine so a ninth field is one array entry away. */
#define HP_MAX_TONE_FIELDS 9
#define HP_MAX_PADS        (HP_MAX_TONE_FIELDS + 1) /* ding + tone fields */

#define HP_MAX_CHORDS    72
#define HP_MAX_FAVORITES 64
#define HP_NAME_LEN      16

/* Chord formula: semitone offsets from the root. Index into hp_formulas[] is
 * persisted in the favorites file, so never reorder this table -- append. */
typedef struct {
    const char* suffix;
    uint8_t tones[6];
    uint8_t count;
} HpFormula;

/* A drum. Notes are MIDI numbers, 60 = C4. */
typedef struct {
    const char* name;
    uint8_t ding;
    const uint8_t* tones; /* ascending */
    uint8_t tone_count;
    bool flats; /* spell accidentals as flats rather than sharps */
} HpScale;

/* A chord derived at runtime for one drum. */
typedef struct {
    uint8_t root_pc; /* root pitch class, 0..11 */
    uint8_t formula; /* index into hp_formulas[] */
    uint16_t pad_mask; /* bit 0 = ding, bits 1..9 = tone fields ascending */
    char name[HP_NAME_LEN];
} HpChord;

typedef struct {
    uint8_t scale;
    uint8_t root_pc;
    uint8_t formula;
} HpFavorite;

#define HP_MAX_STEPS 16

/* One strike in a phrase. Voice `a` always sounds; `b` sounds together with it
 * when it isn't HP_NONE, which is how two-pad strikes are written.
 *
 * A voice is a semitone offset from the drum's ding pitch class rather than an
 * absolute note, so one pattern library serves every drum. HP_DING pins a voice
 * to the ding pad itself: the ding shares the tonic's pitch class, so a plain 0
 * would let the voicer pick whichever octave were nearest, and it almost never
 * chooses the ding. Say HP_DING when you mean the bass voice. */
#define HP_NONE ((int8_t)127)
#define HP_DING ((int8_t) - 1)

typedef struct {
    int8_t a;
    int8_t b;
} HpStep;

#define HP_N(x)     {(int8_t)(x), HP_NONE} /* one tone field */
#define HP_D        {HP_DING, HP_NONE} /* the ding alone */
#define HP_DN(x)    {HP_DING, (int8_t)(x)} /* ding struck with a tone field */
#define HP_NN(x, y) {(int8_t)(x), (int8_t)(y)} /* two tone fields together */

/* A practice phrase. A pattern is simply not offered on a drum that lacks a
 * note it needs. */
typedef struct {
    const char* name;
    const HpStep* steps;
    uint8_t step_count;
} HpPattern;

extern const HpFormula hp_formulas[];
extern const size_t hp_formula_count;
extern const HpScale hp_scales[];
extern const size_t hp_scale_count;
extern const HpPattern hp_patterns[];
extern const size_t hp_pattern_count;

/* scales.c */
const char* hp_pc_name(uint8_t pc, bool flats);
void hp_note_name(uint8_t midi, bool flats, char* out, size_t out_size);
uint8_t hp_scale_pad_count(const HpScale* s);
uint8_t hp_scale_pad_midi(const HpScale* s, uint8_t pad);

/* Derive every chord playable on this drum. max_tones is the depth filter:
 * 3 = triads only, 4 = plus sixths/sevenths, 6 = everything. Returns the
 * number written to out, capped at max. */
size_t hp_build_chords(const HpScale* s, uint8_t max_tones, HpChord* out, size_t max);

/* True when the drum has every pitch class the pattern asks for. */
bool hp_pattern_available(const HpScale* s, const HpPattern* p);

/* Resolve a pattern to one pad mask per step (same numbering as
 * HpChord.pad_mask: bit 0 the ding, bits 1..9 tone fields ascending), voicing
 * the phrase as a whole. Returns the number of steps written. */
size_t hp_pattern_steps(const HpScale* s, const HpPattern* p, uint16_t* out, size_t max);

/* favorites.c */
size_t hp_fav_load(Storage* storage, HpFavorite* out, size_t max);
bool hp_fav_save(Storage* storage, const HpFavorite* favs, size_t count);
