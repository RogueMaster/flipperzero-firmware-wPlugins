#define _POSIX_C_SOURCE 200809L

#include "morse_flipper_progress.h"
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

static uint16_t checked_day(uint16_t year, uint8_t month, uint8_t day) {
    uint16_t out = MORSE_FLIPPER_PROGRESS_DAY_NONE;

    CHECK(morse_flipper_progress_date_to_day(year, month, day, &out));
    CHECK(out != MORSE_FLIPPER_PROGRESS_DAY_NONE);
    return out;
}

static void write_bytes(const char* path, const char* data, size_t len) {
    FILE* f = fopen(path, "wb");

    CHECK(f != NULL);
    CHECK(fwrite(data, 1U, len, f) == len);
    CHECK(fclose(f) == 0);
}

static long file_size(const char* path) {
    struct stat st;

    CHECK(stat(path, &st) == 0);
    return (long)st.st_size;
}

static void test_attempt_rules(void) {
    MorseFlipperProgress progress;
    uint16_t day1 = checked_day(2026, 7, 20);
    uint16_t day2 = checked_day(2026, 7, 21);
    uint16_t day4 = checked_day(2026, 7, 23);

    morse_flipper_progress_reset(&progress);
    CHECK(sizeof(progress) == MORSE_FLIPPER_PROGRESS_SIZE);
    CHECK(progress.version == MORSE_FLIPPER_PROGRESS_VERSION);
    CHECK(progress.last_streak_prompt_day == MORSE_FLIPPER_PROGRESS_DAY_NONE);
    CHECK(morse_flipper_progress_stars(69U) == 0U);
    CHECK(morse_flipper_progress_stars(70U) == 1U);
    CHECK(morse_flipper_progress_stars(85U) == 2U);
    CHECK(morse_flipper_progress_stars(95U) == 3U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day1, 5U, 69U, 20U);
    CHECK(progress.total_attempts == 1U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 1U);
    CHECK(progress.last_streak_day == day1);
    CHECK(progress.last_stats_day == day1);
    CHECK(progress.lesson_attempts[5] == 1U);
    CHECK(progress.lesson_best[5] == 69U);
    CHECK(progress.lesson_last[5] == 69U);

    morse_flipper_progress_note_custom_attempt(&progress, true, day1);
    CHECK(progress.total_attempts == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 1U);
    CHECK(progress.lesson_attempts[5] == 1U);
    CHECK(progress.streak_days == 1U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day1, 5U, 95U, 20U);
    CHECK(progress.total_attempts == 2U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.last_streak_day == day1);
    CHECK(progress.today_attempts == 2U);
    CHECK(progress.daily_record == 2U);
    CHECK(progress.lesson_attempts[5] == 2U);
    CHECK(progress.lesson_best[5] == 95U);
    CHECK(progress.lesson_last[5] == 95U);

    morse_flipper_progress_note_custom_attempt(&progress, true, day2);
    CHECK(progress.streak_days == 2U);
    CHECK(progress.last_streak_day == day2);
    CHECK(progress.today_attempts == 2U);
    CHECK(progress.last_stats_day == day1);
    CHECK(progress.total_attempts == 2U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day4, 6U, 101U, 20U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 2U);
    CHECK(progress.total_attempts == 3U);
    CHECK(progress.lesson_best[6] == 100U);

    morse_flipper_progress_note_standard_attempt(
        &progress, false, MORSE_FLIPPER_PROGRESS_DAY_NONE, 6U, 88U, 20U);
    CHECK(progress.total_attempts == 4U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 2U);
    CHECK(progress.lesson_best[6] == 100U);
    CHECK(progress.lesson_last[6] == 88U);
}

static void test_mastery_progress_rules(void) {
    MorseFlipperProgress progress;
    uint16_t day = checked_day(2026, 7, 20);

    morse_flipper_progress_reset(&progress);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 0U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day, 11U, 100U, 9U);
    CHECK(progress.lesson_mastery_runs[11] == 0U);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 0U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day, 11U, 94U, 20U);
    CHECK(progress.lesson_mastery_runs[11] == 0U);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 0U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day, 11U, 95U, 20U);
    CHECK(progress.lesson_mastery_runs[11] == 1U);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 0U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day, 11U, 100U, 30U);
    CHECK(progress.lesson_mastery_runs[11] == MORSE_FLIPPER_PROGRESS_MASTERY_RUNS);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 11U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day, 7U, 100U, 20U);
    morse_flipper_progress_note_standard_attempt(&progress, true, day, 7U, 100U, 20U);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 11U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day, 12U, 100U, 20U);
    CHECK(morse_flipper_progress_mastered_lesson(&progress) == 11U);
}

static void test_streak_intro_helpers(void) {
    MorseFlipperProgress progress;
    uint16_t day1 = checked_day(2026, 7, 20);
    uint16_t day2 = checked_day(2026, 7, 21);
    uint16_t day4 = checked_day(2026, 7, 23);

    CHECK(morse_flipper_progress_streak_intro_days(NULL, day1) == 1U);
    CHECK(morse_flipper_progress_streak_intro_days(NULL, MORSE_FLIPPER_PROGRESS_DAY_NONE) == 1U);
    CHECK(!morse_flipper_progress_streak_intro_due(NULL, day1));

    morse_flipper_progress_reset(&progress);
    CHECK(morse_flipper_progress_streak_intro_days(&progress, day1) == 1U);
    CHECK(morse_flipper_progress_streak_intro_due(&progress, day1));
    morse_flipper_progress_mark_streak_intro_seen(&progress, MORSE_FLIPPER_PROGRESS_DAY_NONE);
    CHECK(progress.last_streak_prompt_day == MORSE_FLIPPER_PROGRESS_DAY_NONE);
    morse_flipper_progress_mark_streak_intro_seen(&progress, day1);
    CHECK(progress.last_streak_prompt_day == day1);
    CHECK(!morse_flipper_progress_streak_intro_due(&progress, day1));
    CHECK(morse_flipper_progress_streak_intro_due(&progress, day2));

    progress.last_streak_day = day1;
    progress.streak_days = 4U;
    CHECK(morse_flipper_progress_streak_intro_days(&progress, day2) == 5U);
    CHECK(morse_flipper_progress_streak_intro_days(&progress, day4) == 1U);

    progress.last_streak_day = day2;
    progress.streak_days = 6U;
    CHECK(morse_flipper_progress_streak_intro_days(&progress, day2) == 6U);

    progress.last_streak_day = day1;
    progress.streak_days = UINT16_MAX;
    CHECK(morse_flipper_progress_streak_intro_days(&progress, day2) == UINT16_MAX);
}

static void test_weak_letters_are_bounded_and_recent(void) {
    MorseFlipperProgress progress;
    char weak[12];
    char high_byte[2] = {(char)0xFF, '\0'};

    morse_flipper_progress_reset(&progress);
    morse_flipper_progress_note_weak_group(&progress, "KMR", "R", "K");
    morse_flipper_progress_top_weak(&progress, "KMR", weak, sizeof(weak));
    CHECK(strchr(weak, 'R') != NULL);
    CHECK(strchr(weak, 'K') != NULL);
    CHECK(progress.weak_error[(uint8_t)'R'] >= 32U);
    CHECK(progress.weak_error[(uint8_t)'K'] >= 16U);

    morse_flipper_progress_note_weak_group(&progress, high_byte, high_byte, high_byte);

    for(uint8_t i = 0U; i < 6U; i++)
        morse_flipper_progress_note_weak_group(&progress, "KMR", "M", "R");
    morse_flipper_progress_top_weak(&progress, "KMR", weak, sizeof(weak));
    CHECK(strchr(weak, 'M') != NULL);
    CHECK(strchr(weak, 'R') != NULL);
}

static void test_persistence_shape_rules(void) {
    MorseFlipperProgress progress;
    MorseFlipperProgress loaded;
    uint16_t day2 = checked_day(2026, 7, 21);

    CHECK(morse_flipper_progress_load(&loaded));
    CHECK(morse_flipper_progress_valid(&loaded));
    CHECK(loaded.total_attempts == 0U);
    CHECK(loaded.last_streak_prompt_day == MORSE_FLIPPER_PROGRESS_DAY_NONE);

    morse_flipper_progress_reset(&progress);
    morse_flipper_progress_mark_streak_intro_seen(&progress, day2);
    morse_flipper_progress_note_standard_attempt(
        &progress, false, MORSE_FLIPPER_PROGRESS_DAY_NONE, 5U, 91U, 20U);
    CHECK(morse_flipper_progress_save(&progress));
    CHECK(file_size(MORSE_FLIPPER_PROGRESS_PATH) == (long)MORSE_FLIPPER_PROGRESS_SIZE);
    memset(&loaded, 0xA5, sizeof(loaded));
    CHECK(morse_flipper_progress_load(&loaded));
    CHECK(loaded.total_attempts == 1U);
    CHECK(loaded.lesson_best[5] == 91U);
    CHECK(loaded.last_streak_prompt_day == day2);
    CHECK(loaded.lesson_mastery_runs[5] == 0U);

    write_bytes(MORSE_FLIPPER_PROGRESS_PATH, "short", 5U);
    memset(&loaded, 0xA5, sizeof(loaded));
    CHECK(morse_flipper_progress_load(&loaded));
    CHECK(morse_flipper_progress_valid(&loaded));
    CHECK(loaded.total_attempts == 0U);
}

static void test_empty_history_scan_is_resumable(void) {
    MorseFlipperProgressHistoryCursor cursor;
    MorseFlipperProgressHistoryRow row;
    uint16_t today = checked_day(2026, 7, 21);
    uint16_t expected_day = (uint16_t)(today - MORSE_FLIPPER_PROGRESS_HISTORY_SCAN_DAYS);

    morse_flipper_progress_history_reset(&cursor, today);
    CHECK(morse_flipper_progress_history_load_more(&cursor, &row, 1U) == 0U);
    CHECK(!cursor.exhausted);
    CHECK(cursor.practice_day == expected_day);
    while(!cursor.exhausted)
        CHECK(morse_flipper_progress_history_load_more(&cursor, &row, 1U) == 0U);
    CHECK(cursor.practice_day == 0U);
}

static void append_history_day(uint16_t practice_day, uint8_t hour, uint8_t lesson) {
    uint16_t year;
    uint8_t month;
    uint8_t day;

    CHECK(morse_flipper_progress_day_to_date(practice_day, &year, &month, &day));
    CHECK(morse_flipper_progress_append_history(year, month, day, hour, 0U, lesson, 80U));
}

static void test_history_long_gap_continuations(void) {
    static const uint16_t gaps[] = {31U, 32U, 62U, 90U};
    uint16_t base_day = checked_day(2030, 7, 21);

    for(size_t i = 0U; i < sizeof(gaps) / sizeof(gaps[0]); i++) {
        MorseFlipperProgressHistoryCursor older_cursor;
        MorseFlipperProgressHistoryNewerCursor newer_cursor;
        MorseFlipperProgressHistoryRow row;
        uint16_t start = (uint16_t)(base_day - i * 200U);
        uint16_t older_day = (uint16_t)(start - gaps[i] - 1U);
        uint8_t empty_slices = 0U;

        append_history_day(start, (uint8_t)(10U + i), (uint8_t)(20U + i));
        append_history_day(older_day, (uint8_t)(14U + i), (uint8_t)(30U + i));

        morse_flipper_progress_history_reset(&older_cursor, start);
        CHECK(morse_flipper_progress_history_load_more(&older_cursor, &row, 1U) == 1U);
        CHECK(row.practice_day == start);
        while(morse_flipper_progress_history_load_more(&older_cursor, &row, 1U) == 0U) {
            CHECK(!older_cursor.exhausted);
            empty_slices++;
        }
        CHECK(empty_slices > 0U);
        CHECK(row.practice_day == older_day);

        morse_flipper_progress_history_newer_reset(&newer_cursor, &row, start);
        empty_slices = 0U;
        while(!morse_flipper_progress_history_load_newer(&newer_cursor, &row)) {
            CHECK(!newer_cursor.exhausted);
            empty_slices++;
        }
        CHECK(empty_slices > 0U);
        CHECK(row.practice_day == start);
        CHECK(!morse_flipper_progress_history_load_newer(&newer_cursor, &row));
        CHECK(newer_cursor.exhausted);
    }
}

static void test_history_cursor_boundaries_and_reuse(void) {
    MorseFlipperProgressHistoryCursor older_cursor;
    MorseFlipperProgressHistoryNewerCursor newer_cursor;
    MorseFlipperProgressHistoryRow row;
    MorseFlipperProgressHistoryRow anchor;
    uint16_t dec31 = checked_day(2031, 12, 31);
    uint16_t jan1 = checked_day(2032, 1, 1);
    uint16_t jan2 = checked_day(2032, 1, 2);

    append_history_day(dec31, 8U, 40U);
    append_history_day(jan1, 9U, 41U);
    append_history_day(jan1, 10U, 42U);
    append_history_day(jan2, 11U, 43U);

    morse_flipper_progress_history_reset(&older_cursor, jan2);
    CHECK(morse_flipper_progress_history_load_more(&older_cursor, &row, 1U) == 1U);
    CHECK(row.practice_day == jan2);
    CHECK(morse_flipper_progress_history_load_more(&older_cursor, &row, 1U) == 1U);
    CHECK(row.practice_day == jan1 && row.hour == 10U);
    CHECK(morse_flipper_progress_history_load_more(&older_cursor, &row, 1U) == 1U);
    CHECK(row.practice_day == jan1 && row.hour == 9U);
    CHECK(morse_flipper_progress_history_load_more(&older_cursor, &row, 1U) == 1U);
    CHECK(row.practice_day == dec31);

    anchor = row;
    morse_flipper_progress_history_newer_reset(&newer_cursor, &anchor, jan2);
    CHECK(morse_flipper_progress_history_load_newer(&newer_cursor, &row));
    CHECK(row.practice_day == jan1 && row.hour == 9U);
    CHECK(morse_flipper_progress_history_load_newer(&newer_cursor, &row));
    CHECK(row.practice_day == jan1 && row.hour == 10U);
    CHECK(morse_flipper_progress_history_load_newer(&newer_cursor, &row));
    CHECK(row.practice_day == jan2);
    CHECK(!morse_flipper_progress_history_load_newer(&newer_cursor, &row));
    CHECK(newer_cursor.exhausted);

    morse_flipper_progress_history_newer_reset(&newer_cursor, &anchor, jan1);
    CHECK(newer_cursor.initialized && !newer_cursor.exhausted);
    CHECK(morse_flipper_progress_history_load_newer(&newer_cursor, &row));
    CHECK(row.practice_day == jan1 && row.hour == 9U);
    morse_flipper_progress_history_newer_reset(&newer_cursor, NULL, jan2);
    CHECK(newer_cursor.exhausted && !newer_cursor.initialized);
    morse_flipper_progress_history_reset(&older_cursor, MORSE_FLIPPER_PROGRESS_DAY_NONE);
    CHECK(older_cursor.exhausted);
}

static void test_history_start_day_rules(void) {
    MorseFlipperProgress progress;
    MorseFlipperProgressHistoryCursor cursor;
    MorseFlipperProgressHistoryRow row;
    uint16_t today = checked_day(2026, 7, 21);
    uint16_t seeded_day = checked_day(2026, 5, 24);
    uint16_t start;

    morse_flipper_progress_reset(&progress);
    CHECK(morse_flipper_progress_history_start_day(NULL, true, today) == today);
    CHECK(
        morse_flipper_progress_history_start_day(&progress, false, today) ==
        MORSE_FLIPPER_PROGRESS_DAY_NONE);

    progress.last_stats_day = seeded_day;
    CHECK(morse_flipper_progress_history_start_day(&progress, true, today) == seeded_day);
    CHECK(
        morse_flipper_progress_history_start_day(
            &progress, false, MORSE_FLIPPER_PROGRESS_DAY_NONE) == seeded_day);

    CHECK(morse_flipper_progress_append_history(2026, 5, 24, 8, 30, 5, 75));
    start = morse_flipper_progress_history_start_day(&progress, true, today);
    morse_flipper_progress_history_reset(&cursor, start);
    CHECK(morse_flipper_progress_history_load_more(&cursor, &row, 1U) == 1U);
    CHECK(row.practice_day == seeded_day);
    CHECK(row.lesson_idx == 5U);
    CHECK(row.percent == 75U);
}

static void test_history_date_label_cutoff(void) {
    MorseFlipperProgressHistoryRow row;
    char label[8];

    row = (MorseFlipperProgressHistoryRow){
        .practice_day = checked_day(2025, 8, 3),
    };
    morse_flipper_progress_history_date_label(&row, 2026, 7, label, sizeof(label));
    CHECK(strcmp(label, "03 Aug") == 0);

    row.practice_day = checked_day(2025, 7, 3);
    morse_flipper_progress_history_date_label(&row, 2026, 7, label, sizeof(label));
    CHECK(strcmp(label, "Jul 25") == 0);

    row.practice_day = checked_day(2026, 7, 21);
    morse_flipper_progress_history_date_label(&row, 2026, 7, label, sizeof(label));
    CHECK(strcmp(label, "21 Jul") == 0);
}

static void test_history_format_and_bidirectional_loading(void) {
    MorseFlipperProgressHistoryCursor cursor;
    MorseFlipperProgressHistoryRow rows[5];
    MorseFlipperProgressHistoryRow older;
    MorseFlipperProgressHistoryRow newer;
    uint16_t today = checked_day(2026, 7, 21);
    uint16_t day21 = checked_day(2026, 7, 21);
    char path[128];
    FILE* f;
    char file_buf[32];

    CHECK(morse_flipper_progress_append_history(2026, 7, 19, 17, 0, 5, 80));
    CHECK(morse_flipper_progress_append_history(2026, 7, 20, 10, 0, 6, 70));
    CHECK(morse_flipper_progress_append_history(2026, 7, 20, 10, 10, 6, 85));
    CHECK(morse_flipper_progress_append_history(2026, 7, 20, 10, 20, 6, 95));
    CHECK(morse_flipper_progress_append_history(2026, 7, 21, 9, 0, 7, 90));
    CHECK(morse_flipper_progress_append_history(2026, 7, 21, 9, 10, 7, 100));

    CHECK(morse_flipper_progress_history_path(path, sizeof(path), day21));
    f = fopen(path, "rb");
    CHECK(f != NULL);
    CHECK(fread(file_buf, 1U, sizeof(file_buf), f) == 24U);
    CHECK(fclose(f) == 0);
    CHECK(memcmp(file_buf, "0900 07 090\n0910 07 100\n", 24U) == 0);

    morse_flipper_progress_history_reset(&cursor, today);
    CHECK(morse_flipper_progress_history_load_more(&cursor, rows, 5U) == 5U);
    CHECK(rows[0].lesson_idx == 7U && rows[0].hour == 9U && rows[0].minute == 10U);
    CHECK(rows[0].percent == 100U);
    CHECK(rows[1].lesson_idx == 7U && rows[1].hour == 9U && rows[1].minute == 0U);
    CHECK(rows[2].lesson_idx == 6U && rows[2].hour == 10U && rows[2].minute == 20U);
    CHECK(rows[3].lesson_idx == 6U && rows[3].hour == 10U && rows[3].minute == 10U);
    CHECK(rows[4].lesson_idx == 6U && rows[4].hour == 10U && rows[4].minute == 0U);
    CHECK(!cursor.exhausted);

    CHECK(morse_flipper_progress_history_load_more(&cursor, &older, 1U) == 1U);
    CHECK(older.lesson_idx == 5U);
    CHECK(older.hour == 17U);

    MorseFlipperProgressHistoryNewerCursor newer_cursor;

    morse_flipper_progress_history_newer_reset(&newer_cursor, &older, today);
    CHECK(morse_flipper_progress_history_load_newer(&newer_cursor, &newer));
    CHECK(newer.lesson_idx == 6U && newer.hour == 10U && newer.minute == 0U);
    morse_flipper_progress_history_newer_reset(&newer_cursor, &rows[3], today);
    CHECK(morse_flipper_progress_history_load_newer(&newer_cursor, &newer));
    CHECK(newer.lesson_idx == 6U && newer.hour == 10U && newer.minute == 20U);
    morse_flipper_progress_history_newer_reset(&newer_cursor, &rows[0], today);
    CHECK(!morse_flipper_progress_history_load_newer(&newer_cursor, &newer));
}

int main(void) {
    char tmp[] = "/tmp/morse_progress_test_XXXXXX";

    CHECK(mkdtemp(tmp) != NULL);
    CHECK(chdir(tmp) == 0);

    test_attempt_rules();
    test_mastery_progress_rules();
    test_streak_intro_helpers();
    test_weak_letters_are_bounded_and_recent();
    test_persistence_shape_rules();
    test_empty_history_scan_is_resumable();
    test_history_long_gap_continuations();
    test_history_cursor_boundaries_and_reuse();
    test_history_start_day_rules();
    test_history_date_label_cutoff();
    test_history_format_and_bidirectional_loading();

    printf("test_progress: %u checks passed\n", g_checks);
    return 0;
}
