#define _POSIX_C_SOURCE 200809L

#include "morse_flipper_icr.h"
#include "morse_flipper_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned g_checks;

#define CHECK(expr)                                                         \
    do {                                                                    \
        g_checks++;                                                         \
        if(!(expr)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            exit(1);                                                        \
        }                                                                   \
    } while(0)

static long file_size(const char* path) {
    struct stat st;

    CHECK(stat(path, &st) == 0);
    return (long)st.st_size;
}

static void set_level(
    MorseFlipperIcrStats* stats,
    uint8_t target,
    uint8_t candidate,
    uint8_t level) {
    uint8_t* packed = &stats->confusion_levels[target][candidate / 2U];
    uint8_t shift = (uint8_t)((candidate % 2U) * MORSE_FLIPPER_ICR_CONFUSION_BITS);

    *packed = (uint8_t)(*packed & ~(0x0FU << shift));
    *packed = (uint8_t)(*packed | ((level & 0x0FU) << shift));
}

static void clear_row(MorseFlipperIcrStats* stats, uint8_t target) {
    memset(stats->confusion_levels[target], 0, MORSE_FLIPPER_ICR_CONFUSION_ROW_BYTES);
}

static bool choice_contains(
    const uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint8_t candidate) {
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        if(choices[i] == candidate) return true;
    }
    return false;
}

static void assert_choices_valid(
    const uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT],
    uint8_t target) {
    uint8_t target_count = 0U;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        CHECK(choices[i] < MORSE_FLIPPER_ICR_CHAR_COUNT);
        if(choices[i] == target) target_count++;
        for(uint8_t n = 0U; n < i; n++) CHECK(choices[n] != choices[i]);
    }
    CHECK(target_count == 1U);
}

static void test_layout_seeding_and_learning(void) {
    MorseFlipperIcrStats stats;
    uint8_t target = morse_flipper_icr_char_index('A');
    uint8_t wrong = morse_flipper_icr_char_index('X');

    CHECK(MORSE_FLIPPER_ICR_CONFUSION_ROW_BYTES == 20U);
    CHECK(MORSE_FLIPPER_ICR_CONFUSION_BYTES == 800U);
    CHECK(sizeof(stats) == MORSE_FLIPPER_ICR_STATS_SIZE);
    CHECK(sizeof(stats) == 1004U);

    morse_flipper_icr_stats_reset(&stats);
    CHECK(morse_flipper_icr_stats_valid(&stats));
    CHECK(stats.version == MORSE_FLIPPER_ICR_VERSION);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('I')) == 3U);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('N')) == 2U);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('R')) == 1U);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('U')) == 1U);
    target = morse_flipper_icr_char_index('B');
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('6')) == 3U);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('D')) == 3U);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('X')) == 3U);
    CHECK(morse_flipper_icr_confusion_level(
              &stats, target, morse_flipper_icr_char_index('L')) == 2U);
    target = morse_flipper_icr_char_index('A');
    CHECK(morse_flipper_icr_confusion_level(&stats, target, wrong) == 0U);

    morse_flipper_icr_note_answer(&stats, target, target, 400U);
    CHECK(stats.attempts[target] == 1U);
    CHECK(stats.correct[target] == 1U);
    CHECK(stats.avg_ms20[target] == 20U);

    morse_flipper_icr_note_answer(&stats, target, wrong, 600U);
    CHECK(stats.attempts[target] == 2U);
    CHECK(stats.correct[target] == 1U);
    CHECK(stats.avg_ms20[target] == 66U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, wrong) == 1U);

    morse_flipper_icr_note_answer(&stats, target, wrong, 600U);
    morse_flipper_icr_note_answer(&stats, target, wrong, 600U);
    clear_row(&stats, target);
    stats.attempts[target] = 0U;
    for(uint8_t i = 0U; i < 15U; i++)
        morse_flipper_icr_note_answer(&stats, target, wrong, 600U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, wrong) == 15U);
    CHECK(morse_flipper_icr_stats_valid(&stats));
}

static void test_unbounded_personal_admission(void) {
    MorseFlipperIcrStats stats;
    uint8_t target = morse_flipper_icr_char_index('A');
    const char wrong_chars[] = {'B', 'C', 'D', 'E', 'F'};

    morse_flipper_icr_stats_reset(&stats);
    clear_row(&stats, target);
    for(uint8_t i = 0U; i < sizeof(wrong_chars); i++) {
        uint8_t wrong = morse_flipper_icr_char_index(wrong_chars[i]);

        morse_flipper_icr_note_answer(&stats, target, wrong, 500U);
        CHECK(morse_flipper_icr_confusion_level(&stats, target, wrong) == 1U);
    }

    for(uint8_t i = 0U; i < 8U; i++)
        morse_flipper_icr_note_answer(&stats, target, morse_flipper_icr_char_index('B'), 500U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, morse_flipper_icr_char_index('B')) == 9U);
    for(uint8_t i = 1U; i < sizeof(wrong_chars); i++)
        CHECK(morse_flipper_icr_confusion_level(
                  &stats, target, morse_flipper_icr_char_index(wrong_chars[i])) == 1U);
}

static void test_decay_and_reinforcement_order(void) {
    MorseFlipperIcrStats stats;
    uint8_t target = morse_flipper_icr_char_index('A');
    uint8_t wrong = morse_flipper_icr_char_index('X');

    morse_flipper_icr_stats_reset(&stats);
    for(uint8_t i = 0U;
        i < MORSE_FLIPPER_ICR_CONFUSION_DECAY_INTERVAL * 3U;
        i++)
        morse_flipper_icr_note_answer(&stats, target, target, 300U);
    for(uint8_t candidate = 0U; candidate < MORSE_FLIPPER_ICR_CHAR_COUNT; candidate++)
        CHECK(morse_flipper_icr_confusion_level(&stats, target, candidate) == 0U);

    clear_row(&stats, target);
    stats.attempts[target] = MORSE_FLIPPER_ICR_CONFUSION_DECAY_INTERVAL - 1U;
    set_level(&stats, target, wrong, 1U);
    morse_flipper_icr_note_answer(&stats, target, wrong, 500U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, wrong) == 1U);
}

static void test_nibbles_and_personal_dominance(void) {
    MorseFlipperIcrStats stats;
    uint8_t target = morse_flipper_icr_char_index('A');
    uint8_t even = morse_flipper_icr_char_index('B');
    uint8_t odd = morse_flipper_icr_char_index('C');

    morse_flipper_icr_stats_reset(&stats);
    clear_row(&stats, target);
    set_level(&stats, target, even, 4U);
    set_level(&stats, target, odd, 11U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, even) == 4U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, odd) == 11U);
    set_level(&stats, target, even, 15U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, even) == 15U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, odd) == 11U);

    morse_flipper_icr_stats_reset(&stats);
    for(uint8_t i = 0U; i < 6U; i++)
        morse_flipper_icr_note_answer(&stats, target, odd, 500U);
    CHECK(morse_flipper_icr_confusion_level(&stats, target, odd) >
          morse_flipper_icr_confusion_level(&stats, target, morse_flipper_icr_char_index('I')));
}

static void test_answer_composition(void) {
    MorseFlipperIcrStats stats;
    uint32_t rng_a = 0x13579BDFU;
    uint32_t rng_b = 0x13579BDFU;
    uint8_t choices_a[MORSE_FLIPPER_ICR_CHOICE_COUNT];
    uint8_t choices_b[MORSE_FLIPPER_ICR_CHOICE_COUNT];
    uint8_t target = morse_flipper_icr_char_index('A');
    uint8_t first = morse_flipper_icr_char_index('B');
    uint8_t second = morse_flipper_icr_char_index('C');
    uint8_t weighted_count = 0U;
    uint8_t exploration_count;

    morse_flipper_icr_stats_reset(&stats);
    clear_row(&stats, target);
    set_level(&stats, target, first, 3U);
    set_level(&stats, target, second, 1U);
    morse_flipper_icr_build_choices(&stats, target, &rng_a, choices_a);
    morse_flipper_icr_build_choices(&stats, target, &rng_b, choices_b);
    CHECK(memcmp(choices_a, choices_b, sizeof(choices_a)) == 0);
    assert_choices_valid(choices_a, target);
    CHECK(choice_contains(choices_a, first));
    CHECK(choice_contains(choices_a, second));
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        if(choices_a[i] == first || choices_a[i] == second) weighted_count++;
        if(choices_a[i] != target && choices_a[i] != first && choices_a[i] != second)
            CHECK(morse_flipper_icr_confusion_level(&stats, target, choices_a[i]) == 0U);
    }
    CHECK(weighted_count == 2U);

    morse_flipper_icr_stats_reset(&stats);
    rng_a = 0x2468ACE0U;
    morse_flipper_icr_build_choices(&stats, target, &rng_a, choices_a);
    assert_choices_valid(choices_a, target);
    weighted_count = 0U;
    exploration_count = 0U;
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        if(choices_a[i] == target) continue;
        if(morse_flipper_icr_confusion_level(&stats, target, choices_a[i]) > 0U)
            weighted_count++;
        else {
            exploration_count++;
            CHECK(morse_flipper_icr_confusion_level(&stats, target, choices_a[i]) == 0U);
        }
    }
    CHECK(weighted_count == 2U);
    CHECK(exploration_count == 2U);

    clear_row(&stats, target);
    set_level(&stats, target, first, 2U);
    rng_a = 7U;
    morse_flipper_icr_build_choices(&stats, target, &rng_a, choices_a);
    assert_choices_valid(choices_a, target);
    CHECK(choice_contains(choices_a, first));
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        if(choices_a[i] != target && choices_a[i] != first)
            CHECK(morse_flipper_icr_confusion_level(&stats, target, choices_a[i]) == 0U);
    }
}

static void test_validation_and_persistence(void) {
    MorseFlipperIcrStats stats;
    MorseFlipperIcrStats loaded;
    uint8_t target = morse_flipper_icr_char_index('K');
    uint8_t wrong = morse_flipper_icr_char_index('C');

    morse_flipper_icr_stats_reset(&stats);
    set_level(&stats, target, target, 1U);
    CHECK(!morse_flipper_icr_stats_valid(&stats));
    morse_flipper_icr_stats_reset(&stats);
    morse_flipper_icr_note_answer(&stats, target, wrong, 700U);
    CHECK(morse_flipper_icr_stats_save(&stats));
    CHECK(file_size(MORSE_FLIPPER_ICR_STATS_PATH) == MORSE_FLIPPER_ICR_STATS_SIZE);

    memset(&loaded, 0xA5, sizeof(loaded));
    CHECK(morse_flipper_icr_stats_load(&loaded));
    CHECK(morse_flipper_icr_stats_valid(&loaded));
    CHECK(memcmp(&loaded, &stats, sizeof(stats)) == 0);
}

static void test_old_saved_state_resets_without_migration(void) {
    MorseFlipperIcrStats stats;
    MorseFlipperIcrStats loaded;
    FILE* file;
    uint8_t old_state[604] = {0};

    /* Same-size data with the old version is not a compatibility format. */
    morse_flipper_icr_stats_reset(&stats);
    stats.version = MORSE_FLIPPER_ICR_VERSION - 1U;
    file = fopen(MORSE_FLIPPER_ICR_STATS_PATH, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(&stats, 1U, sizeof(stats), file) == sizeof(stats));
    CHECK(fclose(file) == 0);
    CHECK(morse_flipper_icr_stats_load(&loaded));
    CHECK(morse_flipper_icr_stats_valid(&loaded));
    CHECK(loaded.version == MORSE_FLIPPER_ICR_VERSION);
    CHECK(memcmp(&loaded, &stats, sizeof(loaded)) != 0);

    /* The former 2-bit blob size likewise resets to the seeded 4-bit state. */
    memset(old_state, 0xA5, sizeof(old_state));
    file = fopen(MORSE_FLIPPER_ICR_STATS_PATH, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(old_state, 1U, sizeof(old_state), file) == sizeof(old_state));
    CHECK(fclose(file) == 0);
    CHECK(morse_flipper_icr_stats_load(&loaded));
    CHECK(morse_flipper_icr_stats_valid(&loaded));
    CHECK(loaded.version == MORSE_FLIPPER_ICR_VERSION);
    CHECK(morse_flipper_icr_confusion_level(
              &loaded,
              morse_flipper_icr_char_index('A'),
              morse_flipper_icr_char_index('I')) == 3U);
}

static void test_target_selection_contract(void) {
    MorseFlipperIcrStats stats;
    uint32_t rng = 1U;
    uint8_t excluded = morse_flipper_icr_char_index('E');
    uint8_t target;

    morse_flipper_icr_stats_reset(&stats);
    target = morse_flipper_icr_pick_target_except(&stats, &rng, excluded);
    CHECK(target < MORSE_FLIPPER_ICR_CHAR_COUNT);
    CHECK(target != excluded);
}

int main(void) {
    char tmp[] = "/tmp/morse_icr_test_XXXXXX";
    MorseFlipperIcrStats missing;

    CHECK(mkdtemp(tmp) != NULL);
    CHECK(chdir(tmp) == 0);
    CHECK(morse_flipper_icr_stats_load(&missing));
    CHECK(morse_flipper_icr_stats_valid(&missing));

    test_layout_seeding_and_learning();
    test_unbounded_personal_admission();
    test_decay_and_reinforcement_order();
    test_nibbles_and_personal_dominance();
    test_answer_composition();
    test_validation_and_persistence();
    test_old_saved_state_resets_without_migration();
    test_target_selection_contract();

    printf("test_icr: %u checks passed\n", g_checks);
    return 0;
}
