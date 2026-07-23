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

#define MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK 0x03U
#define MORSE_FLIPPER_ICR_PACK_LEVELS(a, b, c, d)                                  \
    ((uint8_t)(((a) & MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK) |                    \
               (((b) & MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK) << 2U) |            \
               (((c) & MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK) << 4U) |            \
               (((d) & MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK) << 6U)))

/*
 * The seed is used only when stats are created or reset.  Each byte stores
 * four 2-bit levels, matching the four candidate characters in its row.
 */

typedef struct {
    char candidate[4];
    uint8_t levels;
} MorseFlipperIcrSeedRow;

static const char morse_flipper_icr_chars[MORSE_FLIPPER_ICR_CHAR_COUNT + 1U] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,/?";

static const MorseFlipperIcrSeedRow morse_flipper_icr_seed_rows[] = {
    {{'I', 'N', 'R', 'U'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 1U, 1U)},
    {{'6', 'D', 'X', 'L'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 3U, 2U)},
    {{'Y', 'P', 'Q', 'K'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'B', 'K', '6', 'L'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'T', 'I', 'S', 'H'}, MORSE_FLIPPER_ICR_PACK_LEVELS(2U, 2U, 2U, 2U)},
    {{'L', 'Y', 'Q', 'P'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'O', 'W', '6', 'Q'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 1U)},
    {{'5', 'S', 'V', 'I'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'A', 'S', 'H', 'E'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'P', '1', 'W', 'Q'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'D', 'X', 'R', 'C'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'F', 'Q', 'Y', 'B'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'N', 'G', 'O', 'T'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 1U, 1U, 1U)},
    {{'M', 'A', 'D', 'K'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 1U, 1U)},
    {{'G', 'Q', '0', 'M'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 1U, 1U)},
    {{'J', 'Q', 'L', 'C'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'Y', 'Z', 'L', 'F'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 3U, 2U)},
    {{'W', 'K', 'L', 'A'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 1U)},
    {{'H', '5', 'I', 'E'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'E', 'M', 'N', 'O'}, MORSE_FLIPPER_ICR_PACK_LEVELS(2U, 1U, 1U, 1U)},
    {{'S', 'V', 'D', 'A'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 1U, 1U)},
    {{'4', 'H', 'U', '5'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'G', 'R', 'J', 'A'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 1U)},
    {{'B', 'Y', '6', 'K'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'Q', 'C', 'F', 'L'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 3U, 2U)},
    {{'Q', '7', 'L', 'G'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'9', '1', '8', 'O'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 1U)},
    {{'2', 'J', '0', '9'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'3', '1', 'S', 'H'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'2', '4', 'H', 'S'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 2U, 2U, 2U)},
    {{'V', '5', '3', 'H'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'H', '4', 'S', 'I'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'B', '7', 'D', 'X'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'6', '8', 'Z', 'B'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'7', '9', 'Z', 'Q'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 2U)},
    {{'0', '8', '1', 'O'}, MORSE_FLIPPER_ICR_PACK_LEVELS(3U, 3U, 2U, 1U)},
    {{',', '/', '?', 'Z'}, MORSE_FLIPPER_ICR_PACK_LEVELS(2U, 2U, 2U, 1U)},
    {{'.', '/', '?', 'Z'}, MORSE_FLIPPER_ICR_PACK_LEVELS(2U, 2U, 2U, 1U)},
    {{'.', ',', '?', 'X'}, MORSE_FLIPPER_ICR_PACK_LEVELS(2U, 2U, 2U, 1U)},
    {{'/', '.', ',', 'Q'}, MORSE_FLIPPER_ICR_PACK_LEVELS(2U, 2U, 2U, 1U)},
};

_Static_assert(
    MORSE_FLIPPER_ICR_COUNT_OF(morse_flipper_icr_seed_rows) ==
        MORSE_FLIPPER_ICR_CHAR_COUNT,
    "ICR seed rows must match the character table");

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

uint8_t morse_flipper_icr_confusion_level(
    const MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t candidate) {
    uint8_t packed;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT ||
       candidate >= MORSE_FLIPPER_ICR_CHAR_COUNT)
        return 0U;

    packed = stats->confusion_levels[target][candidate / 4U];
    return (uint8_t)((packed >> ((candidate % 4U) * MORSE_FLIPPER_ICR_CONFUSION_BITS)) &
                     MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK);
}

static void morse_flipper_icr_set_confusion_level(
    MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t candidate,
    uint8_t level) {
    uint8_t* packed;
    uint8_t shift;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT ||
       candidate >= MORSE_FLIPPER_ICR_CHAR_COUNT)
        return;

    packed = &stats->confusion_levels[target][candidate / 4U];
    shift = (uint8_t)((candidate % 4U) * MORSE_FLIPPER_ICR_CONFUSION_BITS);
    *packed = (uint8_t)(*packed & ~(MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK << shift));
    *packed = (uint8_t)(*packed | ((level & MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK) << shift));
}

static void morse_flipper_icr_seed_confusions(MorseFlipperIcrStats* stats) {
    if(stats == NULL) return;

    for(uint8_t target = 0U; target < MORSE_FLIPPER_ICR_CHAR_COUNT; target++) {
        const MorseFlipperIcrSeedRow* row = &morse_flipper_icr_seed_rows[target];

        for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_COUNT_OF(row->candidate); i++) {
            uint8_t candidate = morse_flipper_icr_char_index(row->candidate[i]);
            uint8_t level =
                (uint8_t)((row->levels >> (i * MORSE_FLIPPER_ICR_CONFUSION_BITS)) &
                          MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK);

            if(candidate != MORSE_FLIPPER_ICR_NO_CHOICE && candidate != target)
                morse_flipper_icr_set_confusion_level(stats, target, candidate, level);
        }
    }
}

static uint8_t morse_flipper_icr_confusion_weight(uint8_t level) {
    static const uint8_t weights[] = {0U, 2U, 6U, 12U};

    return weights[level & MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK];
}

static void morse_flipper_icr_decay_confusions(MorseFlipperIcrStats* stats, uint8_t target) {
    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    for(uint8_t candidate = 0U; candidate < MORSE_FLIPPER_ICR_CHAR_COUNT; candidate++) {
        uint8_t level = morse_flipper_icr_confusion_level(stats, target, candidate);

        if(level > 0U) morse_flipper_icr_set_confusion_level(stats, target, candidate, level - 1U);
    }
}

static void morse_flipper_icr_learn_confusion(
    MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t candidate) {
    uint8_t level;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT ||
       candidate >= MORSE_FLIPPER_ICR_CHAR_COUNT || candidate == target)
        return;

    level = morse_flipper_icr_confusion_level(stats, target, candidate);
    if(level < MORSE_FLIPPER_ICR_CONFUSION_LEVEL_MASK)
        morse_flipper_icr_set_confusion_level(stats, target, candidate, level + 1U);
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
            total = (uint16_t)(total + morse_flipper_icr_confusion_weight(
                                         morse_flipper_icr_confusion_level(stats, target, i)));
    }
    if(total == 0U) return false;

    pick = (uint16_t)morse_flipper_icr_rng_bounded(rng_state, total);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t weight = morse_flipper_icr_confusion_weight(
            morse_flipper_icr_confusion_level(stats, target, i));

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

static uint8_t morse_flipper_icr_random_exploration_choice(
    const MorseFlipperIcrStats* stats,
    uint8_t target,
    const uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint8_t count,
    uint32_t* rng_state) {
    uint8_t candidate;

    for(uint8_t attempts = 0U; attempts < 80U; attempts++) {
        candidate =
            (uint8_t)morse_flipper_icr_rng_bounded(rng_state, MORSE_FLIPPER_ICR_CHAR_COUNT);
        if(candidate != target && !morse_flipper_icr_choice_used(choices, count, candidate) &&
           morse_flipper_icr_confusion_level(stats, target, candidate) == 0U)
            return candidate;
    }

    for(candidate = 0U; candidate < MORSE_FLIPPER_ICR_CHAR_COUNT; candidate++) {
        if(candidate != target && !morse_flipper_icr_choice_used(choices, count, candidate) &&
           morse_flipper_icr_confusion_level(stats, target, candidate) == 0U)
            return candidate;
    }

    /* A fully populated row is unlikely, but still yields five unique answers. */
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

static uint8_t morse_flipper_icr_update_average(uint8_t average, uint8_t sample) {
    if(average == 0U) return sample;
    return (uint8_t)((((uint16_t)average * 4U) + sample + 2U) / 5U);
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

    for(uint8_t target = 0U; target < MORSE_FLIPPER_ICR_CHAR_COUNT; target++) {
        if(stats->correct[target] > stats->attempts[target]) return false;

        if(morse_flipper_icr_confusion_level(stats, target, target) != 0U) return false;
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
            if(ok && !morse_flipper_icr_stats_valid(stats)) {
                morse_flipper_icr_stats_reset(stats);
            }
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

uint8_t morse_flipper_icr_pick_target_except(
    const MorseFlipperIcrStats* stats,
    uint32_t* rng_state,
    uint8_t excluded) {
    uint16_t total = 0U;
    uint16_t pick;

    if(stats == NULL) return 0U;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        if(i == excluded) continue;
        total = (uint16_t)(total + morse_flipper_icr_target_weight(stats, i));
    }
    if(total == 0U) return 0U;

    pick = (uint16_t)morse_flipper_icr_rng_bounded(rng_state, total);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t weight = morse_flipper_icr_target_weight(stats, i);

        if(i == excluded) continue;
        if(pick < weight) return i;
        pick = (uint16_t)(pick - weight);
    }

    return 0U;
}

uint8_t morse_flipper_icr_pick_target(const MorseFlipperIcrStats* stats, uint32_t* rng_state) {
    return morse_flipper_icr_pick_target_except(stats, rng_state, MORSE_FLIPPER_ICR_NO_CHOICE);
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
    while(count < 3U) {
        uint8_t candidate = MORSE_FLIPPER_ICR_NO_CHOICE;

        if(!morse_flipper_icr_pick_confusion(stats, target, choices, count, rng_state, &candidate))
            break;
        choices[count++] = candidate;
    }

    while(count < MORSE_FLIPPER_ICR_CHOICE_COUNT) {
        uint8_t candidate = morse_flipper_icr_random_exploration_choice(
            stats, target, choices, count, rng_state);

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
    uint8_t bucket;
    bool correct;

    if(stats == NULL || target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    correct = choice == target;
    bucket = correct ? morse_flipper_icr_reaction_bucket(reaction_ms) :
                       MORSE_FLIPPER_ICR_TIMEOUT_BUCKET;

    if(stats->attempts[target] < UINT16_MAX) stats->attempts[target]++;
    if(correct && stats->correct[target] < UINT16_MAX) stats->correct[target]++;
    /* Decay before learning so the answer that triggered this interval survives it. */
    if(stats->attempts[target] != 0U &&
       stats->attempts[target] % MORSE_FLIPPER_ICR_CONFUSION_DECAY_INTERVAL == 0U)
        morse_flipper_icr_decay_confusions(stats, target);
    if(!correct) morse_flipper_icr_learn_confusion(stats, target, choice);
    stats->avg_ms20[target] =
        morse_flipper_icr_update_average(stats->avg_ms20[target], bucket);
}
