/*
 * Purpose: Track compact Listening progress and bounded history rows.
 * Owns: progress.bin layout, streak math, weak-letter weights, and history cursors.
 * Depends on: host-safe integer types plus storage paths in the implementation.
 * Tests: firmware build; pure helpers are kept host-safe for future tests.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_FLIPPER_PROGRESS_LESSON_CAP         64U
#define MORSE_FLIPPER_PROGRESS_ASCII_CAP          128U
#define MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN   12U
#define MORSE_FLIPPER_PROGRESS_HISTORY_ROWS       4U
#define MORSE_FLIPPER_PROGRESS_HISTORY_CACHE_ROWS (MORSE_FLIPPER_PROGRESS_HISTORY_ROWS + 4U)
#define MORSE_FLIPPER_PROGRESS_HISTORY_SCAN_DAYS  31U
#define MORSE_FLIPPER_PROGRESS_DAY_NONE           UINT16_MAX
#define MORSE_FLIPPER_PROGRESS_EPOCH_YEAR         2024U
#define MORSE_FLIPPER_PROGRESS_MAX_YEAR           2038U
#define MORSE_FLIPPER_PROGRESS_MAGIC              0x4d50U
#define MORSE_FLIPPER_PROGRESS_VERSION            3U
#define MORSE_FLIPPER_PROGRESS_SIZE               530U
#define MORSE_FLIPPER_PROGRESS_MASTERY_GROUPS     10U
#define MORSE_FLIPPER_PROGRESS_MASTERY_RUNS       2U

typedef enum {
    MorseFlipperProgressPageStats = 0,
    MorseFlipperProgressPageTotals,
    MorseFlipperProgressPageHistory,
} MorseFlipperProgressPage;

typedef struct {
    uint16_t magic;
    uint16_t version;
    uint16_t total_attempts;
    uint16_t streak_days;
    uint16_t daily_record;
    uint16_t today_attempts;
    uint16_t last_streak_day;
    uint16_t last_stats_day;
    uint16_t last_streak_prompt_day;
    uint8_t lesson_attempts[MORSE_FLIPPER_PROGRESS_LESSON_CAP];
    uint8_t lesson_best[MORSE_FLIPPER_PROGRESS_LESSON_CAP];
    uint8_t lesson_last[MORSE_FLIPPER_PROGRESS_LESSON_CAP];
    uint8_t lesson_mastery_runs[MORSE_FLIPPER_PROGRESS_LESSON_CAP];
    uint8_t weak_seen[MORSE_FLIPPER_PROGRESS_ASCII_CAP];
    uint8_t weak_error[MORSE_FLIPPER_PROGRESS_ASCII_CAP];
} MorseFlipperProgress;

_Static_assert(
    sizeof(MorseFlipperProgress) == MORSE_FLIPPER_PROGRESS_SIZE,
    "MorseFlipperProgress size changed");

typedef struct {
    uint16_t practice_day;
    uint16_t line_from_end;
    bool exhausted;
} MorseFlipperProgressHistoryCursor;

typedef struct {
    char lesson;
    uint16_t practice_day;
    uint16_t line_from_end;
    uint8_t hour;
    uint8_t minute;
    uint8_t lesson_idx;
    uint8_t percent;
} MorseFlipperProgressHistoryRow;

void morse_flipper_progress_reset(MorseFlipperProgress* progress);
bool morse_flipper_progress_valid(const MorseFlipperProgress* progress);
uint8_t morse_flipper_progress_stars(uint8_t percent);
uint8_t morse_flipper_progress_best_score(const MorseFlipperProgress* progress);
void morse_flipper_progress_note_standard_attempt(
    MorseFlipperProgress* progress,
    bool date_valid,
    uint16_t practice_day,
    uint8_t lesson,
    uint8_t percent,
    uint8_t session_groups);
void morse_flipper_progress_note_custom_attempt(
    MorseFlipperProgress* progress,
    bool date_valid,
    uint16_t practice_day);
uint8_t morse_flipper_progress_mastered_lesson(const MorseFlipperProgress* progress);
uint16_t morse_flipper_progress_streak_intro_days(
    const MorseFlipperProgress* progress,
    uint16_t practice_day);
bool morse_flipper_progress_streak_intro_due(
    const MorseFlipperProgress* progress,
    uint16_t practice_day);
void morse_flipper_progress_mark_streak_intro_seen(
    MorseFlipperProgress* progress,
    uint16_t practice_day);
void morse_flipper_progress_note_weak_group(
    MorseFlipperProgress* progress,
    const char* active_charset,
    const char* expected,
    const char* answer);
uint8_t morse_flipper_progress_top_weak(
    const MorseFlipperProgress* progress,
    const char* active_charset,
    char* out,
    uint8_t out_cap);

bool morse_flipper_progress_date_to_day(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint16_t* out_day);
bool morse_flipper_progress_day_to_date(
    uint16_t practice_day,
    uint16_t* out_year,
    uint8_t* out_month,
    uint8_t* out_day);
bool morse_flipper_progress_history_path(char* out, size_t out_sz, uint16_t practice_day);
bool morse_flipper_progress_history_row_date(
    const MorseFlipperProgressHistoryRow* row,
    uint16_t* out_year,
    uint8_t* out_month,
    uint8_t* out_day);
void morse_flipper_progress_history_date_label(
    const MorseFlipperProgressHistoryRow* row,
    uint16_t reference_year,
    uint8_t reference_month,
    char* out,
    size_t out_sz);
uint16_t morse_flipper_progress_history_start_day(
    const MorseFlipperProgress* progress,
    bool today_valid,
    uint16_t today);

bool morse_flipper_progress_load(MorseFlipperProgress* progress);
bool morse_flipper_progress_save(const MorseFlipperProgress* progress);
bool morse_flipper_progress_append_history(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t lesson,
    uint8_t percent);
void morse_flipper_progress_history_reset(
    MorseFlipperProgressHistoryCursor* cursor,
    uint16_t practice_day);
uint8_t morse_flipper_progress_history_load_more(
    MorseFlipperProgressHistoryCursor* cursor,
    MorseFlipperProgressHistoryRow* rows,
    uint8_t row_cap);
bool morse_flipper_progress_history_load_newer(
    const MorseFlipperProgressHistoryRow* from,
    uint16_t newest_day,
    MorseFlipperProgressHistoryRow* out);
