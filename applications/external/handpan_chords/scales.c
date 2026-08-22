#include "handpan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Note naming                                                         */
/* ------------------------------------------------------------------ */

static const char* const hp_sharp_names[12] =
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static const char* const hp_flat_names[12] =
    {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

const char* hp_pc_name(uint8_t pc, bool flats) {
    pc = (uint8_t)(pc % 12);
    return flats ? hp_flat_names[pc] : hp_sharp_names[pc];
}

void hp_note_name(uint8_t midi, bool flats, char* out, size_t out_size) {
    if(!out || out_size == 0) return;
    int octave = (int)(midi / 12) - 1;
    snprintf(out, out_size, "%s%d", hp_pc_name((uint8_t)(midi % 12), flats), octave);
}

/* ------------------------------------------------------------------ */
/* Chord formulas                                                      */
/* ------------------------------------------------------------------ */

/* Ordered simplest-first: triads, then sixths/sevenths, then extensions.
 * The index into this table is what favorites.txt stores, so appending is
 * safe but reordering would silently rewrite every saved favorite. */
const HpFormula hp_formulas[] = {
    /* triads */
    {"", {0, 4, 7}, 3},
    {"m", {0, 3, 7}, 3},
    {"dim", {0, 3, 6}, 3},
    {"aug", {0, 4, 8}, 3},
    {"sus2", {0, 2, 7}, 3},
    {"sus4", {0, 5, 7}, 3},
    /* sixths and sevenths */
    {"6", {0, 4, 7, 9}, 4},
    {"m6", {0, 3, 7, 9}, 4},
    {"7", {0, 4, 7, 10}, 4},
    {"maj7", {0, 4, 7, 11}, 4},
    {"m7", {0, 3, 7, 10}, 4},
    {"m7b5", {0, 3, 6, 10}, 4},
    {"add9", {0, 2, 4, 7}, 4},
    {"madd9", {0, 2, 3, 7}, 4},
    /* extensions */
    {"9", {0, 2, 4, 7, 10}, 5},
    {"maj9", {0, 2, 4, 7, 11}, 5},
    {"m9", {0, 2, 3, 7, 10}, 5},
    {"m11", {0, 3, 5, 7, 10}, 5},
};

const size_t hp_formula_count = COUNT_OF(hp_formulas);

/* ------------------------------------------------------------------ */
/* Drums                                                               */
/* ------------------------------------------------------------------ */
/* MIDI note numbers, 60 = C4. Tone fields ascending. */

static const uint8_t hp_d_kurd9[] = {57, 58, 60, 62, 64, 65, 67, 69};
/* D3 | A3 Bb3 C4 D4 E4 F4 G4 A4 */

static const uint8_t hp_d_celtic_min9[] = {57, 60, 62, 64, 65, 67, 69, 72};
/* D3 | A3 C4 D4 E4 F4 G4 A4 C5 */

static const uint8_t hp_d_sabye9[] = {57, 59, 61, 62, 64, 66, 67, 69};
/* D3 | A3 B3 C#4 D4 E4 F#4 G4 A4 */

static const uint8_t hp_cs_amara9[] = {56, 59, 61, 63, 64, 66, 68, 71};
/* C#3 | G#3 B3 C#4 D#4 E4 F#4 G#4 B4 */

static const uint8_t hp_e_la_sirena9[] = {59, 60, 62, 64, 66, 67, 69, 71};
/* E3 | B3 C4 D4 E4 F#4 G4 A4 B4 */

static const uint8_t hp_b_kurd9[] = {54, 55, 57, 59, 61, 62, 64, 66};
/* B2 | F#3 G3 A3 B3 C#4 D4 E4 F#4 */

static const uint8_t hp_a_kurd9[] = {52, 53, 55, 57, 59, 60, 62, 64};
/* A2 | E3 F3 G3 A3 B3 C4 D4 E4 */

static const uint8_t hp_f_low_pygmy9[] = {56, 58, 60, 63, 65, 68, 70, 72};
/* F3 | Ab3 Bb3 C4 Eb4 F4 Ab4 Bb4 C5 */

static const uint8_t hp_c_major9[] = {55, 57, 59, 60, 62, 64, 65, 67};
/* C3 | G3 A3 B3 C4 D4 E4 F4 G4 */

const HpScale hp_scales[] = {
    {"D Kurd 9", 50, hp_d_kurd9, COUNT_OF(hp_d_kurd9), true},
    {"D Celtic Min 9", 50, hp_d_celtic_min9, COUNT_OF(hp_d_celtic_min9), false},
    {"D Sabye 9", 50, hp_d_sabye9, COUNT_OF(hp_d_sabye9), false},
    {"C# Amara 9", 49, hp_cs_amara9, COUNT_OF(hp_cs_amara9), false},
    {"E La Sirena 9", 52, hp_e_la_sirena9, COUNT_OF(hp_e_la_sirena9), false},
    {"B Kurd 9", 47, hp_b_kurd9, COUNT_OF(hp_b_kurd9), false},
    {"A Kurd 9", 45, hp_a_kurd9, COUNT_OF(hp_a_kurd9), false},
    {"F Low Pygmy 9", 53, hp_f_low_pygmy9, COUNT_OF(hp_f_low_pygmy9), true},
    {"C Major 9", 48, hp_c_major9, COUNT_OF(hp_c_major9), false},
};

const size_t hp_scale_count = COUNT_OF(hp_scales);

/* ------------------------------------------------------------------ */
/* Pad access                                                          */
/* ------------------------------------------------------------------ */

uint8_t hp_scale_pad_count(const HpScale* s) {
    if(!s) return 0;
    uint8_t tones = s->tone_count;
    if(tones > HP_MAX_TONE_FIELDS) tones = HP_MAX_TONE_FIELDS;
    return (uint8_t)(tones + 1); /* ding + tone fields */
}

/* Pad 0 is the ding, pads 1..n are the tone fields ascending. */
uint8_t hp_scale_pad_midi(const HpScale* s, uint8_t pad) {
    if(!s) return 0;
    if(pad == 0) return s->ding;
    if(pad > s->tone_count) return 0;
    return s->tones[pad - 1];
}

/* ------------------------------------------------------------------ */
/* Chord derivation                                                    */
/* ------------------------------------------------------------------ */

size_t hp_build_chords(const HpScale* s, uint8_t max_tones, HpChord* out, size_t max) {
    if(!s || !out || max == 0) return 0;

    uint8_t pads[HP_MAX_PADS];
    uint8_t pad_count = hp_scale_pad_count(s);
    for(uint8_t i = 0; i < pad_count; i++) {
        pads[i] = hp_scale_pad_midi(s, i);
    }

    /* Which pitch classes exist anywhere on this drum, ding included. */
    uint16_t pc_mask = 0;
    for(uint8_t i = 0; i < pad_count; i++) {
        pc_mask |= (uint16_t)(1u << (pads[i] % 12));
    }

    size_t n = 0;
    uint8_t ding_pc = (uint8_t)(s->ding % 12);

    /* Start at the ding's pitch class and step up chromatically so the tonic
     * chord lands first; roots the drum doesn't have are skipped. */
    for(uint8_t step = 0; step < 12 && n < max; step++) {
        uint8_t root = (uint8_t)((ding_pc + step) % 12);
        if(!(pc_mask & (uint16_t)(1u << root))) continue;

        for(size_t f = 0; f < hp_formula_count && n < max; f++) {
            const HpFormula* fm = &hp_formulas[f];
            if(fm->count > max_tones) continue;

            /* A chord qualifies only if every one of its tones is present. */
            uint16_t chord_pcs = 0;
            bool playable = true;
            for(uint8_t t = 0; t < fm->count; t++) {
                uint8_t pc = (uint8_t)((root + fm->tones[t]) % 12);
                if(!(pc_mask & (uint16_t)(1u << pc))) {
                    playable = false;
                    break;
                }
                chord_pcs |= (uint16_t)(1u << pc);
            }
            if(!playable) continue;

            /* Light every physical pad carrying one of those pitch classes --
             * a pitch class on two pads (the octave, usually) lights both. */
            uint16_t pad_mask = 0;
            for(uint8_t i = 0; i < pad_count; i++) {
                if(chord_pcs & (uint16_t)(1u << (pads[i] % 12))) {
                    pad_mask |= (uint16_t)(1u << i);
                }
            }

            HpChord* c = &out[n++];
            c->root_pc = root;
            c->formula = (uint8_t)f;
            c->pad_mask = pad_mask;
            snprintf(c->name, sizeof(c->name), "%s%s", hp_pc_name(root, s->flats), fm->suffix);
        }
    }

    return n;
}

/* ------------------------------------------------------------------ */
/* Practice patterns                                                   */
/* ------------------------------------------------------------------ */

/* Steps are semitone offsets from the drum's ding pitch class, written as
 * scale degrees so the same phrase transposes to every drum:
 *   0=1  2=2  3=b3  4=3  5=4  7=5  8=b6  9=6  10=b7  11=7
 * A pattern is offered only on drums that have every degree it uses, so the
 * minor phrases never surface on C Major 9 and vice versa. */

/* A drum's tone fields span only about one octave, so a phrase that keeps
 * walking in one direction runs out of pads and has to jump an octave to carry
 * on. These stay inside a single register, and anything that descends starts
 * high enough to have somewhere to go. */

/* --- minor pentatonic: playable on every minor drum, F Low Pygmy included */
static const HpStep hp_pat_first_steps[] =
    {HP_N(0), HP_N(3), HP_N(5), HP_N(7), HP_N(5), HP_N(3), HP_N(0)};
static const HpStep hp_pat_rolling[] =
    {HP_N(0), HP_N(3), HP_N(7), HP_N(3), HP_N(0), HP_N(3), HP_N(7), HP_N(3), HP_N(0)};
static const HpStep hp_pat_call[] =
    {HP_N(0), HP_N(3), HP_N(5), HP_N(3), HP_N(0), HP_N(7), HP_N(5), HP_N(3), HP_N(0)};
static const HpStep hp_pat_descent[] =
    {HP_N(7), HP_N(5), HP_N(3), HP_N(0), HP_N(10), HP_N(7), HP_N(10), HP_N(0)};
static const HpStep hp_pat_drift[] =
    {HP_N(0), HP_N(7), HP_N(5), HP_N(3), HP_N(5), HP_N(7), HP_N(3), HP_N(0)};

/* --- natural minor without the b6: adds the 2nd */
static const HpStep hp_pat_aeolian[] =
    {HP_N(0), HP_N(2), HP_N(3), HP_N(5), HP_N(7), HP_N(5), HP_N(3), HP_N(2), HP_N(0)};
static const HpStep hp_pat_lament[] =
    {HP_N(7), HP_N(5), HP_N(3), HP_N(2), HP_N(0), HP_N(2), HP_N(3), HP_N(0)};

/* --- full natural minor: needs the b6 */
/* i - VII - VI - VII arpeggiated; on D Kurd that is Dm C Bb C */
static const HpStep hp_pat_minor_cycle[] = {
    HP_N(0),
    HP_N(3),
    HP_N(7),
    HP_N(10),
    HP_N(2),
    HP_N(5),
    HP_N(8),
    HP_N(0),
    HP_N(3),
    HP_N(10),
    HP_N(2),
    HP_N(5)};
/* a full octave down from the 5th: 5 4 b3 2 1 b7 b6 5 */
static const HpStep hp_pat_kurd_descent[] =
    {HP_N(7), HP_N(5), HP_N(3), HP_N(2), HP_N(0), HP_N(10), HP_N(8), HP_N(7)};

/* --- major drums */
static const HpStep hp_pat_sunrise[] =
    {HP_N(0), HP_N(2), HP_N(4), HP_N(5), HP_N(7), HP_N(5), HP_N(4), HP_N(2), HP_N(0)};
static const HpStep hp_pat_pastoral[] =
    {HP_N(0), HP_N(4), HP_N(5), HP_N(7), HP_N(4), HP_N(2), HP_N(0)};
/* I - V - vi - IV arpeggiated; the four-chord pop progression */
static const HpStep hp_pat_four_chords[] = {
    HP_N(0),
    HP_N(4),
    HP_N(7),
    HP_N(2),
    HP_N(7),
    HP_N(11),
    HP_N(9),
    HP_N(0),
    HP_N(4),
    HP_N(5),
    HP_N(9),
    HP_N(0)};
/* I - IV - V - I */
static const HpStep hp_pat_hymn[] = {
    HP_N(0),
    HP_N(4),
    HP_N(7),
    HP_N(5),
    HP_N(9),
    HP_N(0),
    HP_N(7),
    HP_N(11),
    HP_N(2),
    HP_N(0),
    HP_N(4),
    HP_N(7)};
static const HpStep hp_pat_major_roll[] =
    {HP_N(0), HP_N(4), HP_N(7), HP_N(11), HP_N(7), HP_N(4), HP_N(0)};

/* --- the ding and two-pad strikes: how a handpan actually gets played, with
 * the ding as the bass anchor under the melody. Kept after the plain phrases
 * so the list reads simple-first. */

/* Alternate bass and melody -- thumb on the ding, fingers on the fields. */
static const HpStep hp_pat_ding_groove[] =
    {HP_D, HP_N(3), HP_D, HP_N(5), HP_D, HP_N(7), HP_N(5), HP_N(3)};
static const HpStep hp_pat_bass_melody[] =
    {HP_D, HP_N(0), HP_N(3), HP_N(5), HP_D, HP_N(7), HP_N(5), HP_N(0)};
/* Ding struck together with the tonic field an octave up -- the big open sound. */
static const HpStep hp_pat_octave_roll[] =
    {HP_DN(0), HP_N(3), HP_N(5), HP_N(3), HP_DN(0), HP_N(7), HP_N(5), HP_N(3)};
/* Root and fifth together, answered by single notes. */
static const HpStep hp_pat_open_fifths[] =
    {HP_NN(0, 7), HP_N(3), HP_N(5), HP_NN(0, 7), HP_N(7), HP_N(5), HP_N(3), HP_NN(0, 7)};
/* Ding pedal under moving dyads: i - VII - VI - VII. */
static const HpStep hp_pat_drone_cycle[] =
    {HP_D, HP_NN(0, 3), HP_D, HP_NN(10, 2), HP_D, HP_NN(8, 0), HP_D, HP_NN(10, 2)};
/* --- major, with the ding */
static const HpStep hp_pat_bass_walk[] =
    {HP_D, HP_N(4), HP_N(7), HP_D, HP_N(5), HP_N(4), HP_N(2), HP_N(0)};
static const HpStep hp_pat_ding_thirds[] =
    {HP_D, HP_NN(0, 4), HP_D, HP_NN(2, 5), HP_D, HP_NN(4, 7), HP_NN(2, 5), HP_NN(0, 4)};

#define HP_PAT(arr) arr, COUNT_OF(arr)

const HpPattern hp_patterns[] = {
    {"First Steps", HP_PAT(hp_pat_first_steps)},
    {"Rolling Arp", HP_PAT(hp_pat_rolling)},
    {"Call & Answer", HP_PAT(hp_pat_call)},
    {"Descent", HP_PAT(hp_pat_descent)},
    {"Drift", HP_PAT(hp_pat_drift)},
    {"Aeolian Walk", HP_PAT(hp_pat_aeolian)},
    {"Lament", HP_PAT(hp_pat_lament)},
    {"Minor Cycle", HP_PAT(hp_pat_minor_cycle)},
    {"Kurd Descent", HP_PAT(hp_pat_kurd_descent)},
    {"Sunrise", HP_PAT(hp_pat_sunrise)},
    {"Pastoral", HP_PAT(hp_pat_pastoral)},
    {"Four Chords", HP_PAT(hp_pat_four_chords)},
    {"Hymn", HP_PAT(hp_pat_hymn)},
    {"Major Roll", HP_PAT(hp_pat_major_roll)},
    /* ding and two-pad strikes */
    {"Ding Groove", HP_PAT(hp_pat_ding_groove)},
    {"Bass & Melody", HP_PAT(hp_pat_bass_melody)},
    {"Octave Roll", HP_PAT(hp_pat_octave_roll)},
    {"Open Fifths", HP_PAT(hp_pat_open_fifths)},
    {"Drone Cycle", HP_PAT(hp_pat_drone_cycle)},
    {"Bass Walk", HP_PAT(hp_pat_bass_walk)},
    {"Ding & Thirds", HP_PAT(hp_pat_ding_thirds)},
};

const size_t hp_pattern_count = COUNT_OF(hp_patterns);

/* Large enough to outweigh any distance a phrase could cover, so it acts as a
 * hard preference rather than a nudge. */
#define HP_DING_PENALTY 200

static uint16_t hp_scale_pc_mask(const HpScale* s) {
    uint16_t mask = 0;
    uint8_t pad_count = hp_scale_pad_count(s);
    for(uint8_t i = 0; i < pad_count; i++) {
        mask |= (uint16_t)(1u << (hp_scale_pad_midi(s, i) % 12));
    }
    return mask;
}

static bool hp_voice_available(uint16_t pc_mask, uint8_t tonic, int8_t voice) {
    if(voice == HP_NONE || voice == HP_DING) return true; /* every drum has a ding */
    uint8_t pc = (uint8_t)((tonic + voice) % 12);
    return (pc_mask & (uint16_t)(1u << pc)) != 0;
}

/* Can `pad` sound this voice? */
static bool hp_voice_pad(const HpScale* s, uint8_t tonic, int8_t voice, uint8_t pad) {
    if(voice == HP_DING) return pad == 0;
    uint8_t pc = (uint8_t)((tonic + voice) % 12);
    return (hp_scale_pad_midi(s, pad) % 12) == pc;
}

bool hp_pattern_available(const HpScale* s, const HpPattern* p) {
    if(!s || !p) return false;

    uint16_t pc_mask = hp_scale_pc_mask(s);
    uint8_t tonic = (uint8_t)(s->ding % 12);

    for(uint8_t i = 0; i < p->step_count; i++) {
        if(!hp_voice_available(pc_mask, tonic, p->steps[i].a)) return false;
        if(!hp_voice_available(pc_mask, tonic, p->steps[i].b)) return false;
    }
    return true;
}

/* Choosing each step's nearest pad is only locally optimal: it happily walks a
 * descending line into the bottom of the drum and then has to leap an octave
 * back up to continue. So pick the whole phrase at once -- a small shortest-path
 * over (step, pad) that minimises the total distance the hands travel. */
size_t hp_pattern_steps(const HpScale* s, const HpPattern* p, uint16_t* out, size_t max) {
    if(!s || !p || !out || max == 0 || p->step_count == 0) return 0;

    uint8_t pad_count = hp_scale_pad_count(s);
    uint8_t tonic = (uint8_t)(s->ding % 12);
    uint8_t steps = p->step_count;
    if(steps > HP_MAX_STEPS) steps = HP_MAX_STEPS;

    /* The ding is the bass voice, not part of the tune, so the shortest path
     * runs over the tone-field steps only and ding steps pass through. Include
     * them and a phrase that alternates bass and melody drags the melody down
     * into the ding's octave -- the 5th of a rising line lands an octave low
     * because that pad happens to sit nearer the ding. */
    uint8_t melodic[HP_MAX_STEPS];
    uint8_t melodic_count = 0;
    for(uint8_t i = 0; i < steps; i++) {
        if(p->steps[i].a != HP_DING) melodic[melodic_count++] = i;
    }

    uint8_t lead[HP_MAX_STEPS];
    for(uint8_t i = 0; i < steps; i++) {
        lead[i] = 0; /* ding steps are pinned; melodic steps filled in below */
    }

    if(melodic_count > 0) {
        int16_t cost[HP_MAX_STEPS][HP_MAX_PADS];
        uint8_t back[HP_MAX_STEPS][HP_MAX_PADS];
        bool usable[HP_MAX_STEPS][HP_MAX_PADS];

        for(uint8_t k = 0; k < melodic_count; k++) {
            int8_t voice = p->steps[melodic[k]].a;
            bool any = false;

            for(uint8_t pad = 0; pad < pad_count; pad++) {
                usable[k][pad] = hp_voice_pad(s, tonic, voice, pad);
                if(!usable[k][pad]) continue;
                any = true;

                /* Keep the tune off the ding -- ask for it with HP_DING. This
                 * outweighs any distance, so the ding is only picked when no
                 * tone field carries the note at all. */
                int ding_penalty = (pad == 0) ? HP_DING_PENALTY : 0;

                if(k == 0) {
                    cost[k][pad] = (int16_t)ding_penalty;
                    back[k][pad] = pad;
                    continue;
                }

                int midi = (int)hp_scale_pad_midi(s, pad);
                int best = -1;
                uint8_t best_prev = 0;
                for(uint8_t q = 0; q < pad_count; q++) {
                    if(!usable[k - 1][q]) continue;
                    int c = cost[k - 1][q] + 4 * abs(midi - (int)hp_scale_pad_midi(s, q));
                    if(best < 0 || c < best) {
                        best = c;
                        best_prev = q;
                    }
                }
                if(best < 0) return 0;
                cost[k][pad] = (int16_t)(best + ding_penalty);
                back[k][pad] = best_prev;
            }

            if(!any) return 0; /* drum can't play this step */
        }

        int best = -1;
        uint8_t cur = 0;
        for(uint8_t pad = 0; pad < pad_count; pad++) {
            if(!usable[melodic_count - 1][pad]) continue;
            if(best < 0 || cost[melodic_count - 1][pad] < best) {
                best = cost[melodic_count - 1][pad];
                cur = pad;
            }
        }
        if(best < 0) return 0;

        for(int k = melodic_count - 1; k >= 0; k--) {
            lead[melodic[k]] = cur;
            cur = back[k][cur];
        }
    }

    size_t n = steps;
    if(n > max) n = max;

    for(size_t i = 0; i < n; i++) {
        uint8_t pad_a = lead[i];
        uint16_t mask = (uint16_t)(1u << pad_a);

        int8_t second = p->steps[i].b;
        if(second != HP_NONE) {
            int lead_midi = (int)hp_scale_pad_midi(s, pad_a);
            int best_pad = -1;
            int best_gap = 0;

            /* The second voice sits on whichever pad carrying its note is
             * closest to the one already committed to -- close pads are what
             * you can actually strike together. */
            for(uint8_t pad = 0; pad < pad_count; pad++) {
                if(pad == pad_a) continue;
                if(!hp_voice_pad(s, tonic, second, pad)) continue;

                int gap = abs((int)hp_scale_pad_midi(s, pad) - lead_midi);
                if(pad == 0 && second != HP_DING) gap += HP_DING_PENALTY;
                if(best_pad < 0 || gap < best_gap) {
                    best_pad = (int)pad;
                    best_gap = gap;
                }
            }

            /* If the drum carries that note on one pad only and the lead is
             * already using it, the strike just sounds as a single note. */
            if(best_pad >= 0) mask |= (uint16_t)(1u << best_pad);
        }

        out[i] = mask;
    }

    return n;
}
