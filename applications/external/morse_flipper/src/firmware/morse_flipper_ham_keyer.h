/*
 * Purpose: Define the host-safe ham keyer data model.
 * Owns: message limits, assignment IDs, macro state, and public keyer API.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_ham_keyer.c.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES  12U
#define MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN   48U
#define MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS   5U
#define MORSE_FLIPPER_HAM_KEYER_UNASSIGNED    0xFFU
#define MORSE_FLIPPER_HAM_KEYER_PENDING_LEN   384U
#define MORSE_FLIPPER_HAM_KEYER_DATE_LEN      10U
#define MORSE_FLIPPER_HAM_KEYER_STAMP_LEN     16U
#define MORSE_FLIPPER_HAM_KEYER_INACTIVITY_MS 4000U

typedef enum {
    MorseFlipperHamKeyerDirUp = 0,
    MorseFlipperHamKeyerDirDown,
    MorseFlipperHamKeyerDirLeft,
    MorseFlipperHamKeyerDirRight,
    MorseFlipperHamKeyerDirOk,
} MorseFlipperHamKeyerDir;

typedef struct {
    bool logging_enabled;
    uint8_t message_count;
    char messages[MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES][MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN + 1U];
    uint8_t assignments[MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS];
    bool break_in_enabled;
    char pending[MORSE_FLIPPER_HAM_KEYER_PENDING_LEN];
    size_t pending_len;
    uint32_t pending_started_at_ms;
    uint32_t last_activity_at_ms;
    char current_log_date[MORSE_FLIPPER_HAM_KEYER_DATE_LEN + 1U];
    char pending_stamp[MORSE_FLIPPER_HAM_KEYER_STAMP_LEN + 1U];
} MorseFlipperHamKeyer;

void morse_flipper_ham_keyer_reset(MorseFlipperHamKeyer* keyer);
void morse_flipper_ham_keyer_normalize(MorseFlipperHamKeyer* keyer);
bool morse_flipper_ham_keyer_add_message(MorseFlipperHamKeyer* keyer, const char* message);
bool morse_flipper_ham_keyer_edit_message(
    MorseFlipperHamKeyer* keyer,
    uint8_t index,
    const char* message);
bool morse_flipper_ham_keyer_delete_message(MorseFlipperHamKeyer* keyer, uint8_t index);
bool morse_flipper_ham_keyer_duplicate_message(
    MorseFlipperHamKeyer* keyer,
    uint8_t index,
    uint8_t* new_index);
bool morse_flipper_ham_keyer_assign(
    MorseFlipperHamKeyer* keyer,
    uint8_t dir,
    uint8_t message_index);
const char*
    morse_flipper_ham_keyer_assignment_text(const MorseFlipperHamKeyer* keyer, uint8_t dir);
const char* morse_flipper_ham_keyer_dir_label(uint8_t dir);
bool morse_flipper_ham_keyer_append_activity(
    MorseFlipperHamKeyer* keyer,
    const char* text,
    uint32_t now_ms,
    const char* date_key,
    const char* stamp);
bool morse_flipper_ham_keyer_append_marker(
    MorseFlipperHamKeyer* keyer,
    const char* marker,
    uint32_t now_ms,
    const char* date_key,
    const char* stamp);
bool morse_flipper_ham_keyer_should_flush(const MorseFlipperHamKeyer* keyer, uint32_t now_ms);
void morse_flipper_ham_keyer_clear_pending(MorseFlipperHamKeyer* keyer);
