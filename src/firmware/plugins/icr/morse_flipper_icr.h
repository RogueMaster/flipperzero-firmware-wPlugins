/*
 * Purpose: Publish ICR training stats and adaptive selection helpers.
 * Owns: ICR character pool, stats blob layout, target weighting, and choices.
 * Depends on: host-safe integer types plus storage paths in the implementation.
 * Tests: firmware build.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MORSE_FLIPPER_ICR_CHAR_COUNT        40U
#define MORSE_FLIPPER_ICR_CHOICE_COUNT      5U
#define MORSE_FLIPPER_ICR_CONFUSION_BITS     2U
#define MORSE_FLIPPER_ICR_CONFUSION_ROW_BYTES \
    ((MORSE_FLIPPER_ICR_CHAR_COUNT * MORSE_FLIPPER_ICR_CONFUSION_BITS) / 8U)
#define MORSE_FLIPPER_ICR_CONFUSION_BYTES \
    (MORSE_FLIPPER_ICR_CHAR_COUNT * MORSE_FLIPPER_ICR_CONFUSION_ROW_BYTES)
/* Each target's mutable confusion row loses one level every 16 attempts. */
#define MORSE_FLIPPER_ICR_CONFUSION_DECAY_INTERVAL 16U
#define MORSE_FLIPPER_ICR_MAGIC             0x4943U
#define MORSE_FLIPPER_ICR_VERSION           2U
#define MORSE_FLIPPER_ICR_STATS_SIZE        604U
#define MORSE_FLIPPER_ICR_TIMEOUT_BUCKET    250U
#define MORSE_FLIPPER_ICR_NO_CHOICE         0xFFU
#define MORSE_FLIPPER_ICR_INSTANT_BUCKET    30U
#define MORSE_FLIPPER_ICR_RECOGNIZED_BUCKET 100U

typedef struct {
    uint16_t magic;
    uint16_t version;
    uint16_t attempts[MORSE_FLIPPER_ICR_CHAR_COUNT];
    uint16_t correct[MORSE_FLIPPER_ICR_CHAR_COUNT];
    uint8_t avg_ms20[MORSE_FLIPPER_ICR_CHAR_COUNT];
    /* 40 directed rows x 40 packed 2-bit answer levels = 400 bytes. */
    uint8_t confusion_levels[MORSE_FLIPPER_ICR_CHAR_COUNT]
                            [MORSE_FLIPPER_ICR_CONFUSION_ROW_BYTES];
} MorseFlipperIcrStats;

_Static_assert(
    sizeof(MorseFlipperIcrStats) == MORSE_FLIPPER_ICR_STATS_SIZE,
    "MorseFlipperIcrStats size changed");

void morse_flipper_icr_stats_reset(MorseFlipperIcrStats* stats);
bool morse_flipper_icr_stats_valid(const MorseFlipperIcrStats* stats);
bool morse_flipper_icr_stats_load(MorseFlipperIcrStats* stats);
bool morse_flipper_icr_stats_save(const MorseFlipperIcrStats* stats);

char morse_flipper_icr_char_at(uint8_t index);
uint8_t morse_flipper_icr_char_index(char ch);
uint8_t morse_flipper_icr_reaction_bucket(uint32_t reaction_ms);
uint8_t morse_flipper_icr_confusion_level(
    const MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t candidate);
uint8_t morse_flipper_icr_target_weight(const MorseFlipperIcrStats* stats, uint8_t index);
uint8_t morse_flipper_icr_pick_target(const MorseFlipperIcrStats* stats, uint32_t* rng_state);
uint8_t morse_flipper_icr_pick_target_except(
    const MorseFlipperIcrStats* stats,
    uint32_t* rng_state,
    uint8_t excluded);
void morse_flipper_icr_build_choices(
    const MorseFlipperIcrStats* stats,
    uint8_t target,
    uint32_t* rng_state,
    uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT]);
void morse_flipper_icr_note_answer(
    MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t choice,
    uint32_t reaction_ms);
