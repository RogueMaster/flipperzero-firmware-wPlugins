/*
 * Purpose: Implement ICR training stats and adaptive selection helpers.
 * Owns: ICR persistence, confusion priors, target weights, and answer choices.
 * Depends on: morse_flipper_icr.h, morse_flipper_paths.h, and storage APIs.
 * Tests: firmware build.
 */

#include "morse_flipper_icr.h"
#include "morse_flipper_paths.h"

#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <storage/storage.h>
#else
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#endif

#define MORSE_FLIPPER_ICR_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    char target;
    char candidate[4];
    uint8_t weight[4];
} MorseFlipperIcrSeedRow;

static const char morse_flipper_icr_chars[MORSE_FLIPPER_ICR_CHAR_COUNT + 1U] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,/?";

static const MorseFlipperIcrSeedRow morse_flipper_icr_seed_rows[] = {
    {'A', {'I', 'N', 'R', 'U'}, {9U, 5U, 4U, 4U}},
    {'B', {'6', 'D', 'X', 'L'}, {14U, 11U, 11U, 8U}},
    {'C', {'Y', 'P', 'Q', 'K'}, {11U, 7U, 5U, 5U}},
    {'D', {'B', 'K', '6', 'L'}, {11U, 11U, 8U, 8U}},
    {'E', {'T', 'I', 'S', 'H'}, {7U, 5U, 5U, 5U}},
    {'F', {'L', 'Y', 'Q', 'P'}, {9U, 9U, 8U, 7U}},
    {'G', {'O', 'W', '6', 'Q'}, {9U, 9U, 5U, 4U}},
    {'H', {'5', 'S', 'V', 'I'}, {13U, 11U, 8U, 6U}},
    {'I', {'A', 'S', 'H', 'E'}, {9U, 6U, 6U, 5U}},
    {'J', {'P', '1', 'W', 'Q'}, {11U, 8U, 6U, 5U}},
    {'K', {'D', 'X', 'R', 'C'}, {11U, 8U, 6U, 5U}},
    {'L', {'F', 'Q', 'Y', 'B'}, {9U, 9U, 8U, 8U}},
    {'M', {'N', 'G', 'O', 'T'}, {9U, 4U, 3U, 3U}},
    {'N', {'M', 'A', 'D', 'K'}, {9U, 5U, 4U, 4U}},
    {'O', {'G', 'Q', '0', 'M'}, {9U, 5U, 4U, 3U}},
    {'P', {'J', 'Q', 'L', 'C'}, {11U, 8U, 7U, 7U}},
    {'Q', {'Y', 'Z', 'L', 'F'}, {12U, 10U, 9U, 8U}},
    {'R', {'W', 'K', 'L', 'A'}, {9U, 6U, 5U, 4U}},
    {'S', {'H', '5', 'I', 'E'}, {11U, 8U, 6U, 5U}},
    {'T', {'E', 'M', 'N', 'O'}, {7U, 3U, 3U, 3U}},
    {'U', {'S', 'V', 'D', 'A'}, {9U, 7U, 4U, 4U}},
    {'V', {'4', 'H', 'U', '5'}, {13U, 8U, 7U, 7U}},
    {'W', {'G', 'R', 'J', 'A'}, {9U, 9U, 6U, 4U}},
    {'X', {'B', 'Y', '6', 'K'}, {11U, 9U, 8U, 8U}},
    {'Y', {'Q', 'C', 'F', 'L'}, {12U, 11U, 9U, 8U}},
    {'Z', {'Q', '7', 'L', 'G'}, {10U, 8U, 8U, 5U}},
    {'0', {'9', '1', '8', 'O'}, {11U, 8U, 5U, 4U}},
    {'1', {'2', 'J', '0', '9'}, {10U, 8U, 8U, 8U}},
    {'2', {'3', '1', 'S', 'H'}, {10U, 10U, 5U, 5U}},
    {'3', {'2', '4', 'H', 'S'}, {10U, 8U, 6U, 5U}},
    {'4', {'V', '5', '3', 'H'}, {13U, 9U, 8U, 5U}},
    {'5', {'H', '4', 'S', 'I'}, {13U, 9U, 8U, 5U}},
    {'6', {'B', '7', 'D', 'X'}, {14U, 10U, 8U, 8U}},
    {'7', {'6', '8', 'Z', 'B'}, {10U, 10U, 8U, 7U}},
    {'8', {'7', '9', 'Z', 'Q'}, {10U, 9U, 7U, 7U}},
    {'9', {'0', '8', '1', 'O'}, {11U, 9U, 8U, 4U}},
    {'.', {',', '/', '?', 'Z'}, {8U, 8U, 8U, 4U}},
    {',', {'.', '/', '?', 'Z'}, {8U, 8U, 8U, 4U}},
    {'/', {'.', ',', '?', 'X'}, {8U, 8U, 8U, 4U}},
    {'?', {'/', '.', ',', 'Q'}, {8U, 8U, 8U, 4U}},
};

static uint32_t morse_flipper_icr_rng_next(uint32_t* state) {
    uint32_t next = state != NULL && *state != 0U ? *state : 0x6d2b79f5U;

    next = next * 1664525UL + 1013904223UL;
    if(state != NULL) *state = next;
    return next;
}

static uint32_t morse_flipper_icr_rng_bounded(uint32_t* state, uint32_t limit) {
    if(limit == 0U) return 0U;
    return morse_flipper_icr_rng_next(state) % limit;
}

static void morse_flipper_icr_seed_confusions(MorseFlipperIcrStats* stats) {
    if(stats == NULL) return;

    memset(stats->confusion_weight, 0, sizeof(stats->confusion_weight));
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_COUNT_OF(morse_flipper_icr_seed_rows); i++) {
        uint8_t target = morse_flipper_icr_char_index(morse_flipper_icr_seed_rows[i].target);

        if(target == MORSE_FLIPPER_ICR_NO_CHOICE) continue;
        for(uint8_t n = 0U;
            n < MORSE_FLIPPER_ICR_COUNT_OF(morse_flipper_icr_seed_rows[i].candidate);
            n++) {
            uint8_t candidate =
                morse_flipper_icr_char_index(morse_flipper_icr_seed_rows[i].candidate[n]);

            if(candidate == MORSE_FLIPPER_ICR_NO_CHOICE || candidate == target) continue;
            stats->confusion_weight[target][candidate] = morse_flipper_icr_seed_rows[i].weight[n];
        }
    }
}

static bool morse_flipper_icr_choice_used(
    const uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint8_t count,
    uint8_t candidate) {
    for(uint8_t i = 0U; i < count; i++) {
        if(choices[i] == candidate) return true;
    }
    return false;
}

static bool morse_flipper_icr_pick_confusion(
    const MorseFlipperIcrStats* stats,
    uint8_t target,
    const uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint8_t count,
    uint32_t* rng_state,
    uint8_t* out) {
    uint16_t total = 0U;
    uint16_t pick;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT || out == NULL) return false;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        if(i != target && !morse_flipper_icr_choice_used(choices, count, i))
            total = (uint16_t)(total + stats->confusion_weight[target][i]);
    }
    if(total == 0U) return false;

    pick = (uint16_t)morse_flipper_icr_rng_bounded(rng_state, total);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t weight = stats->confusion_weight[target][i];

        if(i == target || weight == 0U || morse_flipper_icr_choice_used(choices, count, i))
            continue;
        if(pick < weight) {
            *out = i;
            return true;
        }
        pick = (uint16_t)(pick - weight);
    }

    return false;
}

static uint8_t morse_flipper_icr_random_unused_choice(
    uint8_t target,
    const uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint8_t count,
    uint32_t* rng_state) {
    uint8_t candidate;

    for(uint8_t attempts = 0U; attempts < 80U; attempts++) {
        candidate =
            (uint8_t)morse_flipper_icr_rng_bounded(rng_state, MORSE_FLIPPER_ICR_CHAR_COUNT);
        if(candidate != target && !morse_flipper_icr_choice_used(choices, count, candidate))
            return candidate;
    }

    for(candidate = 0U; candidate < MORSE_FLIPPER_ICR_CHAR_COUNT; candidate++) {
        if(candidate != target && !morse_flipper_icr_choice_used(choices, count, candidate))
            return candidate;
    }

    return MORSE_FLIPPER_ICR_NO_CHOICE;
}

static void morse_flipper_icr_shuffle_choices(
    uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint32_t* rng_state) {
    for(uint8_t i = MORSE_FLIPPER_ICR_CHOICE_COUNT - 1U; i > 0U; i--) {
        uint8_t j = (uint8_t)morse_flipper_icr_rng_bounded(rng_state, (uint32_t)i + 1U);
        uint8_t tmp = choices[i];

        choices[i] = choices[j];
        choices[j] = tmp;
    }
}

static void morse_flipper_icr_recompute_average(MorseFlipperIcrStats* stats, uint8_t target) {
    uint16_t total = 0U;
    uint8_t count;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    count = stats->recent_count[target];
    if(count > MORSE_FLIPPER_ICR_RECENT_COUNT) count = MORSE_FLIPPER_ICR_RECENT_COUNT;
    if(count == 0U) {
        stats->avg_ms20[target] = 0U;
        return;
    }

    for(uint8_t i = 0U; i < count; i++) {
        total = (uint16_t)(total + stats->recent_ms20[target][i]);
    }
    stats->avg_ms20[target] = (uint8_t)((total + (count / 2U)) / count);
}

void morse_flipper_icr_stats_reset(MorseFlipperIcrStats* stats) {
    if(stats == NULL) return;

    memset(stats, 0, sizeof(*stats));
    stats->magic = MORSE_FLIPPER_ICR_MAGIC;
    stats->version = MORSE_FLIPPER_ICR_VERSION;
    morse_flipper_icr_seed_confusions(stats);
}

bool morse_flipper_icr_stats_valid(const MorseFlipperIcrStats* stats) {
    if(stats == NULL) return false;
    if(stats->magic != MORSE_FLIPPER_ICR_MAGIC || stats->version != MORSE_FLIPPER_ICR_VERSION)
        return false;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        if(stats->recent_count[i] > MORSE_FLIPPER_ICR_RECENT_COUNT) return false;
        if(stats->recent_pos[i] >= MORSE_FLIPPER_ICR_RECENT_COUNT) return false;
        if(stats->correct[i] > stats->attempts[i]) return false;
    }

    return true;
}

#ifndef MORSE_FLIPPER_FAP
static void morse_flipper_icr_host_mkdirs(void) {
    mkdir("ext", 0777);
    mkdir("ext/apps_data", 0777);
    mkdir(MORSE_FLIPPER_APP_DATA_DIR, 0777);
}
#endif

bool morse_flipper_icr_stats_load(MorseFlipperIcrStats* stats) {
    if(stats == NULL) return false;
    morse_flipper_icr_stats_reset(stats);

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    FileInfo info = {0};
    FS_Error stat_err;
    uint16_t got = 0U;
    bool ok = true;

    stat_err = storage_common_stat(storage, MORSE_FLIPPER_ICR_STATS_PATH, &info);
    if(stat_err == FSE_NOT_EXIST) {
        ok = true;
    } else if(stat_err != FSE_OK) {
        ok = false;
    } else if(file_info_is_dir(&info)) {
        morse_flipper_icr_stats_reset(stats);
    } else if(info.size == MORSE_FLIPPER_ICR_STATS_SIZE) {
        if(storage_file_open(file, MORSE_FLIPPER_ICR_STATS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            got = storage_file_read(file, stats, sizeof(*stats));
            if(got != sizeof(*stats)) ok = false;
            if(ok && !morse_flipper_icr_stats_valid(stats)) morse_flipper_icr_stats_reset(stats);
        } else {
            ok = false;
        }
    } else {
        morse_flipper_icr_stats_reset(stats);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!ok) morse_flipper_icr_stats_reset(stats);
    return ok;
#else
    FILE* f = fopen(MORSE_FLIPPER_ICR_STATS_PATH, "rb");
    long size;
    size_t got;

    if(f == NULL) return errno == ENOENT;
    if(fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    size = ftell(f);
    rewind(f);
    if(size == (long)MORSE_FLIPPER_ICR_STATS_SIZE) {
        got = fread(stats, 1U, sizeof(*stats), f);
        fclose(f);
        if(got != sizeof(*stats)) {
            morse_flipper_icr_stats_reset(stats);
            return false;
        }
        if(!morse_flipper_icr_stats_valid(stats)) morse_flipper_icr_stats_reset(stats);
        return true;
    }

    fclose(f);
    morse_flipper_icr_stats_reset(stats);
    return true;
#endif
}

bool morse_flipper_icr_stats_save(const MorseFlipperIcrStats* stats) {
    if(!morse_flipper_icr_stats_valid(stats)) return false;

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint16_t wrote = 0U;

    storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
    if(storage_file_open(file, MORSE_FLIPPER_ICR_STATS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        wrote = storage_file_write(file, stats, sizeof(*stats));
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return wrote == sizeof(*stats);
#else
    FILE* f;
    size_t wrote;

    morse_flipper_icr_host_mkdirs();
    f = fopen(MORSE_FLIPPER_ICR_STATS_PATH, "wb");
    if(f == NULL) return false;
    wrote = fwrite(stats, 1U, sizeof(*stats), f);
    fclose(f);
    return wrote == sizeof(*stats);
#endif
}

char morse_flipper_icr_char_at(uint8_t index) {
    if(index >= MORSE_FLIPPER_ICR_CHAR_COUNT) return '?';
    return morse_flipper_icr_chars[index];
}

uint8_t morse_flipper_icr_char_index(char ch) {
    if(ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        if(morse_flipper_icr_chars[i] == ch) return i;
    }

    return MORSE_FLIPPER_ICR_NO_CHOICE;
}

uint8_t morse_flipper_icr_reaction_bucket(uint32_t reaction_ms) {
    uint32_t bucket = (reaction_ms + 19U) / 20U;

    if(bucket == 0U) bucket = 1U;
    if(bucket > MORSE_FLIPPER_ICR_TIMEOUT_BUCKET) bucket = MORSE_FLIPPER_ICR_TIMEOUT_BUCKET;
    return (uint8_t)bucket;
}

uint8_t morse_flipper_icr_target_weight(const MorseFlipperIcrStats* stats, uint8_t index) {
    uint8_t sample;
    uint16_t weight;

    if(stats == NULL || index >= MORSE_FLIPPER_ICR_CHAR_COUNT) return 0U;

    sample = stats->avg_ms20[index];
    if(sample == 0U) {
        weight = 96U;
    } else if(sample <= 30U) {
        weight = 16U;
    } else if(sample <= 100U) {
        weight = (uint16_t)(32U + (((uint16_t)(sample - 30U) * 32U) / 70U));
    } else {
        weight = (uint16_t)(64U + (((uint16_t)(sample - 100U) * 64U) / 150U));
    }

    if(weight > 128U) weight = 128U;
    return (uint8_t)weight;
}

uint8_t morse_flipper_icr_pick_target(const MorseFlipperIcrStats* stats, uint32_t* rng_state) {
    uint16_t total = 0U;
    uint16_t pick;

    if(stats == NULL) return 0U;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        total = (uint16_t)(total + morse_flipper_icr_target_weight(stats, i));
    }
    if(total == 0U) return 0U;

    pick = (uint16_t)morse_flipper_icr_rng_bounded(rng_state, total);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t weight = morse_flipper_icr_target_weight(stats, i);

        if(pick < weight) return i;
        pick = (uint16_t)(pick - weight);
    }

    return 0U;
}

void morse_flipper_icr_build_choices(
    const MorseFlipperIcrStats* stats,
    uint8_t target,
    uint32_t* rng_state,
    uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT]) {
    uint8_t count = 0U;

    if(choices == NULL) return;
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        choices[i] = MORSE_FLIPPER_ICR_NO_CHOICE;
    }
    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    choices[count++] = target;
    while(count < 4U) {
        uint8_t candidate = MORSE_FLIPPER_ICR_NO_CHOICE;

        if(!morse_flipper_icr_pick_confusion(stats, target, choices, count, rng_state, &candidate))
            break;
        choices[count++] = candidate;
    }

    while(count < MORSE_FLIPPER_ICR_CHOICE_COUNT) {
        uint8_t candidate =
            morse_flipper_icr_random_unused_choice(target, choices, count, rng_state);

        if(candidate == MORSE_FLIPPER_ICR_NO_CHOICE) break;
        choices[count++] = candidate;
    }

    morse_flipper_icr_shuffle_choices(choices, rng_state);
}

void morse_flipper_icr_note_answer(
    MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t choice,
    uint32_t reaction_ms) {
    uint8_t pos;
    uint8_t bucket;
    bool correct;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    correct = choice == target;
    bucket = correct ? morse_flipper_icr_reaction_bucket(reaction_ms) :
                       MORSE_FLIPPER_ICR_TIMEOUT_BUCKET;

    if(stats->attempts[target] < UINT16_MAX) stats->attempts[target]++;
    if(correct && stats->correct[target] < UINT16_MAX) stats->correct[target]++;
    if(!correct && choice < MORSE_FLIPPER_ICR_CHAR_COUNT) {
        uint8_t old = stats->confusion_weight[target][choice];

        stats->confusion_weight[target][choice] = old > 251U ? 255U : (uint8_t)(old + 4U);
    }

    pos = stats->recent_pos[target];
    if(pos >= MORSE_FLIPPER_ICR_RECENT_COUNT) pos = 0U;
    stats->recent_ms20[target][pos] = bucket;
    pos = (uint8_t)((pos + 1U) % MORSE_FLIPPER_ICR_RECENT_COUNT);
    stats->recent_pos[target] = pos;
    if(stats->recent_count[target] < MORSE_FLIPPER_ICR_RECENT_COUNT) stats->recent_count[target]++;
    morse_flipper_icr_recompute_average(stats, target);
}
