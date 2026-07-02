/*
 * Purpose: Manage stored ham keyer messages and macro playback queues.
 * Owns: message slots, assignments, pending text, and log stamp formatting.
 * Depends on: morse_flipper_ham_keyer.h.
 * Tests: tests/test_ham_keyer.c.
 */

#include "morse_flipper_ham_keyer.h"

#include <string.h>

static const char* const morse_flipper_ham_keyer_default_messages[] = {
    "CQ CQ CQ POTA DE FL1PPR K",
    "UR RST 5NN 5NN BK",
    "P2P",
    "TU FER PARK ES 73",
    "MY PARK RO 0038 RO 0038 BK",
};

static const char* const morse_flipper_ham_keyer_dir_labels[] = {
    "U",
    "D",
    "L",
    "R",
    "OK",
};

static void morse_flipper_ham_keyer_copy_message(char* dst, const char* src) {
    size_t len;

    if(dst == NULL) return;
    dst[0] = '\0';
    if(src == NULL) return;

    len = strlen(src);
    if(len > MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN) len = MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN;

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void morse_flipper_ham_keyer_seed_defaults(MorseFlipperHamKeyer* keyer) {
    if(keyer == NULL) return;

    keyer->logging_enabled = true;
    keyer->message_count = (uint8_t)(sizeof(morse_flipper_ham_keyer_default_messages) /
                                     sizeof(morse_flipper_ham_keyer_default_messages[0]));

    for(uint8_t i = 0U; i < MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES; i++) {
        keyer->messages[i][0] = '\0';
    }

    for(uint8_t i = 0U; i < keyer->message_count; i++) {
        morse_flipper_ham_keyer_copy_message(
            keyer->messages[i], morse_flipper_ham_keyer_default_messages[i]);
    }

    keyer->assignments[MorseFlipperHamKeyerDirUp] = 2U;
    keyer->assignments[MorseFlipperHamKeyerDirDown] = 1U;
    keyer->assignments[MorseFlipperHamKeyerDirLeft] = 4U;
    keyer->assignments[MorseFlipperHamKeyerDirRight] = 3U;
    keyer->assignments[MorseFlipperHamKeyerDirOk] = 0U;
}

void morse_flipper_ham_keyer_reset(MorseFlipperHamKeyer* keyer) {
    if(keyer == NULL) return;

    memset(keyer, 0, sizeof(*keyer));
    keyer->break_in_enabled = true;
    morse_flipper_ham_keyer_seed_defaults(keyer);
}

void morse_flipper_ham_keyer_normalize(MorseFlipperHamKeyer* keyer) {
    bool any_message = false;

    if(keyer == NULL) return;

    if(keyer->message_count > MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES) {
        morse_flipper_ham_keyer_reset(keyer);
        return;
    }

    for(uint8_t i = 0U; i < keyer->message_count; i++) {
        keyer->messages[i][MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN] = '\0';
        if(keyer->messages[i][0] != '\0') any_message = true;
    }

    if(keyer->message_count == 0U || !any_message) {
        morse_flipper_ham_keyer_reset(keyer);
        return;
    }

    for(uint8_t i = keyer->message_count; i < MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES; i++) {
        keyer->messages[i][0] = '\0';
    }

    for(uint8_t i = 0U; i < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS; i++) {
        if(keyer->assignments[i] >= keyer->message_count) {
            keyer->assignments[i] = MORSE_FLIPPER_HAM_KEYER_UNASSIGNED;
        }
    }

    keyer->pending[0] = '\0';
    keyer->pending_len = 0U;
    keyer->pending_started_at_ms = 0U;
    keyer->last_activity_at_ms = 0U;
    keyer->current_log_date[0] = '\0';
    keyer->pending_stamp[0] = '\0';
}

bool morse_flipper_ham_keyer_add_message(MorseFlipperHamKeyer* keyer, const char* message) {
    if(keyer == NULL || message == NULL || message[0] == '\0') return false;
    if(keyer->message_count >= MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES) return false;

    morse_flipper_ham_keyer_copy_message(keyer->messages[keyer->message_count], message);
    keyer->message_count++;
    morse_flipper_ham_keyer_normalize(keyer);
    return true;
}

bool morse_flipper_ham_keyer_edit_message(
    MorseFlipperHamKeyer* keyer,
    uint8_t index,
    const char* message) {
    if(keyer == NULL || message == NULL || message[0] == '\0') return false;
    if(index >= keyer->message_count) return false;

    morse_flipper_ham_keyer_copy_message(keyer->messages[index], message);
    morse_flipper_ham_keyer_normalize(keyer);
    return true;
}

bool morse_flipper_ham_keyer_delete_message(MorseFlipperHamKeyer* keyer, uint8_t index) {
    if(keyer == NULL || index >= keyer->message_count) return false;

    for(uint8_t i = index; i + 1U < keyer->message_count; i++) {
        morse_flipper_ham_keyer_copy_message(keyer->messages[i], keyer->messages[i + 1U]);
    }

    keyer->messages[keyer->message_count - 1U][0] = '\0';
    keyer->message_count--;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS; i++) {
        if(keyer->assignments[i] == index) {
            keyer->assignments[i] = MORSE_FLIPPER_HAM_KEYER_UNASSIGNED;
        } else if(
            keyer->assignments[i] != MORSE_FLIPPER_HAM_KEYER_UNASSIGNED &&
            keyer->assignments[i] > index) {
            keyer->assignments[i]--;
        }
    }

    morse_flipper_ham_keyer_normalize(keyer);
    return true;
}

bool morse_flipper_ham_keyer_duplicate_message(
    MorseFlipperHamKeyer* keyer,
    uint8_t index,
    uint8_t* new_index) {
    uint8_t dst;

    if(keyer == NULL || index >= keyer->message_count) return false;
    if(keyer->message_count >= MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES) return false;

    dst = keyer->message_count;
    morse_flipper_ham_keyer_copy_message(keyer->messages[dst], keyer->messages[index]);
    keyer->message_count++;
    morse_flipper_ham_keyer_normalize(keyer);
    if(new_index != NULL) *new_index = dst;
    return true;
}

bool morse_flipper_ham_keyer_assign(
    MorseFlipperHamKeyer* keyer,
    uint8_t dir,
    uint8_t message_index) {
    if(keyer == NULL || dir >= MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) return false;
    if(message_index >= keyer->message_count) return false;

    keyer->assignments[dir] = message_index;
    return true;
}

const char*
    morse_flipper_ham_keyer_assignment_text(const MorseFlipperHamKeyer* keyer, uint8_t dir) {
    uint8_t index;

    if(keyer == NULL || dir >= MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) return "";

    index = keyer->assignments[dir];
    if(index >= keyer->message_count) return "";
    return keyer->messages[index];
}

const char* morse_flipper_ham_keyer_dir_label(uint8_t dir) {
    if(dir >= MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) return "?";
    return morse_flipper_ham_keyer_dir_labels[dir];
}

static bool morse_flipper_ham_keyer_pending_append(MorseFlipperHamKeyer* keyer, const char* text) {
    size_t len;
    size_t left;
    size_t add_len;

    if(keyer == NULL || text == NULL || text[0] == '\0') return false;
    if(keyer->pending_len >= MORSE_FLIPPER_HAM_KEYER_PENDING_LEN - 1U) return false;

    len = strlen(text);
    left = (MORSE_FLIPPER_HAM_KEYER_PENDING_LEN - 1U) - keyer->pending_len;
    add_len = len < left ? len : left;
    memcpy(keyer->pending + keyer->pending_len, text, add_len);
    keyer->pending_len += add_len;
    keyer->pending[keyer->pending_len] = '\0';
    return add_len == len;
}

static void morse_flipper_ham_keyer_begin_pending(
    MorseFlipperHamKeyer* keyer,
    uint32_t now_ms,
    const char* date_key,
    const char* stamp) {
    if(keyer == NULL || keyer->pending_len != 0U) return;

    keyer->pending_started_at_ms = now_ms;
    keyer->last_activity_at_ms = now_ms;
    strncpy(keyer->current_log_date, date_key ? date_key : "", MORSE_FLIPPER_HAM_KEYER_DATE_LEN);
    strncpy(keyer->pending_stamp, stamp ? stamp : "", MORSE_FLIPPER_HAM_KEYER_STAMP_LEN);
    keyer->current_log_date[MORSE_FLIPPER_HAM_KEYER_DATE_LEN] = '\0';
    keyer->pending_stamp[MORSE_FLIPPER_HAM_KEYER_STAMP_LEN] = '\0';
}

bool morse_flipper_ham_keyer_append_activity(
    MorseFlipperHamKeyer* keyer,
    const char* text,
    uint32_t now_ms,
    const char* date_key,
    const char* stamp) {
    if(keyer == NULL || text == NULL || text[0] == '\0') return false;

    morse_flipper_ham_keyer_begin_pending(keyer, now_ms, date_key, stamp);
    keyer->last_activity_at_ms = now_ms;
    return morse_flipper_ham_keyer_pending_append(keyer, text);
}

bool morse_flipper_ham_keyer_append_marker(
    MorseFlipperHamKeyer* keyer,
    const char* marker,
    uint32_t now_ms,
    const char* date_key,
    const char* stamp) {
    bool ok = true;

    if(keyer == NULL || marker == NULL || marker[0] == '\0') return false;

    morse_flipper_ham_keyer_begin_pending(keyer, now_ms, date_key, stamp);
    keyer->last_activity_at_ms = now_ms;
    if(keyer->pending_len != 0U && keyer->pending[keyer->pending_len - 1U] != '\n')
        ok = morse_flipper_ham_keyer_pending_append(keyer, "\n");
    ok = morse_flipper_ham_keyer_pending_append(keyer, marker) && ok;
    ok = morse_flipper_ham_keyer_pending_append(keyer, "\n") && ok;
    return ok;
}

bool morse_flipper_ham_keyer_should_flush(const MorseFlipperHamKeyer* keyer, uint32_t now_ms) {
    if(keyer == NULL || keyer->pending_len == 0U) return false;
    return now_ms - keyer->last_activity_at_ms >= MORSE_FLIPPER_HAM_KEYER_INACTIVITY_MS;
}

void morse_flipper_ham_keyer_clear_pending(MorseFlipperHamKeyer* keyer) {
    if(keyer == NULL) return;

    keyer->pending[0] = '\0';
    keyer->pending_len = 0U;
    keyer->pending_started_at_ms = 0U;
    keyer->last_activity_at_ms = 0U;
    keyer->current_log_date[0] = '\0';
    keyer->pending_stamp[0] = '\0';
}
