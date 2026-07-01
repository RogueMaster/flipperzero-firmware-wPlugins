/*
 * Purpose: Define TX Groups training state and scoring API.
 * Owns: group length limits, edge buffers, results, and trainer phases.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_tx_groups.c.
 */

#ifndef yo3gnd_txgroup_90pq
#define yo3gnd_txgroup_90pq

#include <stdbool.h>
#include <stdint.h>

#define MORSE_FLIPPER_TX_GROUP_LEN       5U
#define MORSE_FLIPPER_TX_GROUP_MAX_EDGES 64U

typedef struct {
    uint8_t correct;
    uint8_t speed_pct;
    uint8_t letter_gap_pct;
    uint16_t ratio_x100;
    uint8_t accuracy_pct;
    uint8_t dit_gap_pct;
    uint8_t variance_pct;
    uint8_t dit_pct;
    uint8_t dah_pct;
    bool correct_pass;
    bool speed_pass;
    bool letter_gap_pass;
    bool ratio_pass;
    bool accuracy_pass;
    bool dit_gap_pass;
    bool variance_pass;
    bool passed;
    bool timed_out;
    const char* fault;
} MorseFlipperTxGroupResult;

typedef struct {
    char target[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    char answer[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    uint16_t marks[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint16_t spaces[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint8_t mark_count;
    uint8_t space_count;
    uint8_t pass_min;
    uint8_t pass_max;
    uint32_t rng;
    MorseFlipperTxGroupResult result;
    bool sk;
} MorseFlipperTxGroup;

void morse_flipper_tx_group_init(MorseFlipperTxGroup* g);
void morse_flipper_tx_group_set_seed(MorseFlipperTxGroup* g, uint32_t seed);
void morse_flipper_tx_group_start(MorseFlipperTxGroup* g, bool sk);
void morse_flipper_tx_group_feed_mark(MorseFlipperTxGroup* g, uint16_t ms);
void morse_flipper_tx_group_feed_space(MorseFlipperTxGroup* g, uint16_t ms);
void morse_flipper_tx_group_feed_text(MorseFlipperTxGroup* g, const char* text);
bool morse_flipper_tx_group_finalize_answer_from_raw(MorseFlipperTxGroup* g, uint16_t dit_ms);
void morse_flipper_tx_group_set_range(MorseFlipperTxGroup* g, uint8_t pass_min, uint8_t pass_max);
void morse_flipper_tx_group_score(MorseFlipperTxGroup* g, uint16_t dit_ms, bool timed_out);
void morse_flipper_tx_group_score_common(MorseFlipperTxGroup* g, uint16_t dit_ms, bool timed_out);
bool morse_flipper_tx_group_complete(const MorseFlipperTxGroup* g);
bool morse_flipper_tx_group_marks_complete(const MorseFlipperTxGroup* g);
uint8_t morse_flipper_tx_group_expected_marks(const MorseFlipperTxGroup* g);
uint8_t morse_flipper_tx_group_answer_len(const MorseFlipperTxGroup* g);

#endif
