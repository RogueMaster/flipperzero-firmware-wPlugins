/*
 * Purpose: Persist and summarise compact Listening progress.
 * Owns: progress math, progress.bin IO, history row IO, and app-scoped guards.
 * Depends on: morse_flipper_progress.h, trainer.h, paths, and Flipper storage in FAP builds.
 * Tests: firmware build; pure helpers are host-safe for future tests.
 */

#include "morse_flipper_progress.h"
#include "morse_flipper_paths.h"
#include "trainer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include "morse_flipper_app_i.h"
#else
#include <errno.h>
#include <sys/stat.h>
#endif

static const char* const morse_flipper_progress_months[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
};

static bool morse_flipper_progress_leap(uint16_t year) {
    return (year % 4U) == 0U && ((year % 100U) != 0U || (year % 400U) == 0U);
}

static uint8_t morse_flipper_progress_month_days(uint16_t year, uint8_t month) {
    static const uint8_t days[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if(month < 1U || month > 12U) return 0U;
    if(month == 2U && morse_flipper_progress_leap(year)) return 29U;
    return days[month - 1U];
}

static uint16_t morse_flipper_progress_sat_inc16(uint16_t value) {
    return value == UINT16_MAX ? value : (uint16_t)(value + 1U);
}

static uint8_t morse_flipper_progress_sat_add8(uint8_t value, uint8_t add) {
    uint16_t next = (uint16_t)value + add;
    return next > UINT8_MAX ? UINT8_MAX : (uint8_t)next;
}

static char morse_flipper_progress_upper(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static bool morse_flipper_progress_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

static bool morse_flipper_progress_char_idx(char ch, uint8_t* out_idx) {
    uint8_t idx = (uint8_t)morse_flipper_progress_upper(ch);

    if(idx >= MORSE_FLIPPER_PROGRESS_ASCII_CAP) return false;
    if(out_idx != NULL) *out_idx = idx;
    return true;
}

static bool morse_flipper_progress_charset_contains(const char* charset, char ch) {
    uint8_t ch_idx;

    if(charset == NULL) return false;
    if(!morse_flipper_progress_char_idx(ch, &ch_idx)) return false;

    while(*charset != '\0') {
        uint8_t idx;
        if(morse_flipper_progress_char_idx(*charset, &idx) && idx == ch_idx) return true;
        charset++;
    }

    return false;
}

static char morse_flipper_progress_lesson_letter(uint8_t lesson) {
    char label[16];
    char* dash;

    morse_trainer_lesson_label(lesson, label, sizeof(label));
    dash = strchr(label, '-');
    if(dash == NULL) return '?';
    dash++;
    while(*dash == ' ')
        dash++;
    return *dash != '\0' ? *dash : '?';
}

static uint8_t morse_flipper_progress_clamp_lesson(uint8_t lesson) {
    uint8_t max_lesson = (uint8_t)morse_trainer_lesson_count();

    if(lesson < 1U) lesson = 1U;
    if(lesson > max_lesson) lesson = max_lesson;
    if(lesson >= MORSE_FLIPPER_PROGRESS_LESSON_CAP)
        lesson = MORSE_FLIPPER_PROGRESS_LESSON_CAP - 1U;
    return lesson;
}

void morse_flipper_progress_reset(MorseFlipperProgress* progress) {
    if(progress == NULL) return;

    memset(progress, 0, sizeof(*progress));
    progress->magic = MORSE_FLIPPER_PROGRESS_MAGIC;
    progress->version = MORSE_FLIPPER_PROGRESS_VERSION;
    progress->last_streak_day = MORSE_FLIPPER_PROGRESS_DAY_NONE;
    progress->last_stats_day = MORSE_FLIPPER_PROGRESS_DAY_NONE;
    progress->last_streak_prompt_day = MORSE_FLIPPER_PROGRESS_DAY_NONE;
}

bool morse_flipper_progress_valid(const MorseFlipperProgress* progress) {
    return progress != NULL && progress->magic == MORSE_FLIPPER_PROGRESS_MAGIC &&
           progress->version == MORSE_FLIPPER_PROGRESS_VERSION;
}

uint8_t morse_flipper_progress_stars(uint8_t percent) {
    if(percent >= 95U) return 3U;
    if(percent >= 85U) return 2U;
    if(percent >= 70U) return 1U;
    return 0U;
}

uint8_t morse_flipper_progress_best_score(const MorseFlipperProgress* progress) {
    uint8_t best = 0U;
    uint8_t i;

    if(progress == NULL) return 0U;
    for(i = 0U; i < MORSE_FLIPPER_PROGRESS_LESSON_CAP; i++) {
        if(progress->lesson_best[i] > best) best = progress->lesson_best[i];
    }
    return best;
}

static void morse_flipper_progress_note_streak(MorseFlipperProgress* progress, uint16_t day) {
    if(progress->last_streak_day == MORSE_FLIPPER_PROGRESS_DAY_NONE) {
        progress->streak_days = 1U;
    } else if(day == progress->last_streak_day) {
        if(progress->streak_days == 0U) progress->streak_days = 1U;
    } else if(day == (uint16_t)(progress->last_streak_day + 1U)) {
        progress->streak_days = morse_flipper_progress_sat_inc16(progress->streak_days);
    } else {
        progress->streak_days = 1U;
    }

    progress->last_streak_day = day;
}

static void morse_flipper_progress_note_daily(MorseFlipperProgress* progress, uint16_t day) {
    if(day == progress->last_stats_day) {
        progress->today_attempts = morse_flipper_progress_sat_inc16(progress->today_attempts);
    } else {
        progress->today_attempts = 1U;
        progress->last_stats_day = day;
    }

    if(progress->today_attempts > progress->daily_record)
        progress->daily_record = progress->today_attempts;
}

void morse_flipper_progress_note_standard_attempt(
    MorseFlipperProgress* progress,
    bool date_valid,
    uint16_t practice_day,
    uint8_t lesson,
    uint8_t percent,
    uint8_t session_groups) {
    if(progress == NULL) return;

    if(percent > 100U) percent = 100U;
    lesson = morse_flipper_progress_clamp_lesson(lesson);
    progress->total_attempts = morse_flipper_progress_sat_inc16(progress->total_attempts);
    if(progress->lesson_attempts[lesson] < 40U) progress->lesson_attempts[lesson]++;
    if(percent > progress->lesson_best[lesson]) progress->lesson_best[lesson] = percent;
    progress->lesson_last[lesson] = percent;
    if(percent >= 95U && session_groups >= MORSE_FLIPPER_PROGRESS_MASTERY_GROUPS &&
       progress->lesson_mastery_runs[lesson] < MORSE_FLIPPER_PROGRESS_MASTERY_RUNS) {
        progress->lesson_mastery_runs[lesson]++;
    }

    if(date_valid && practice_day != MORSE_FLIPPER_PROGRESS_DAY_NONE) {
        morse_flipper_progress_note_streak(progress, practice_day);
        morse_flipper_progress_note_daily(progress, practice_day);
    }
}

void morse_flipper_progress_note_custom_attempt(
    MorseFlipperProgress* progress,
    bool date_valid,
    uint16_t practice_day) {
    if(progress == NULL) return;
    if(date_valid && practice_day != MORSE_FLIPPER_PROGRESS_DAY_NONE)
        morse_flipper_progress_note_streak(progress, practice_day);
}

uint8_t morse_flipper_progress_mastered_lesson(const MorseFlipperProgress* progress) {
    uint8_t lesson;

    if(progress == NULL) return 0U;
    lesson = (uint8_t)morse_trainer_lesson_count();
    if(lesson >= MORSE_FLIPPER_PROGRESS_LESSON_CAP)
        lesson = MORSE_FLIPPER_PROGRESS_LESSON_CAP - 1U;

    while(lesson > 0U) {
        if(progress->lesson_mastery_runs[lesson] >= MORSE_FLIPPER_PROGRESS_MASTERY_RUNS)
            return lesson;
        lesson--;
    }

    return 0U;
}

uint16_t morse_flipper_progress_streak_intro_days(
    const MorseFlipperProgress* progress,
    uint16_t practice_day) {
    if(progress == NULL || practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE) return 1U;
    if(progress->last_streak_day == practice_day)
        return progress->streak_days == 0U ? 1U : progress->streak_days;
    if(progress->last_streak_day != MORSE_FLIPPER_PROGRESS_DAY_NONE &&
       (uint16_t)(progress->last_streak_day + 1U) == practice_day)
        return morse_flipper_progress_sat_inc16(progress->streak_days);
    return 1U;
}

bool morse_flipper_progress_streak_intro_due(
    const MorseFlipperProgress* progress,
    uint16_t practice_day) {
    if(progress == NULL) return false;
    if(practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE) return false;
    if(progress->last_streak_prompt_day == practice_day) return false;
    return true;
}

void morse_flipper_progress_mark_streak_intro_seen(
    MorseFlipperProgress* progress,
    uint16_t practice_day) {
    if(progress == NULL) return;
    if(practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE) return;
    progress->last_streak_prompt_day = practice_day;
}

void morse_flipper_progress_note_weak_group(
    MorseFlipperProgress* progress,
    const char* active_charset,
    const char* expected,
    const char* answer) {
    size_t expected_len;
    size_t answer_len;
    size_t i;

    if(progress == NULL || active_charset == NULL || expected == NULL || answer == NULL) return;
    expected_len = strlen(expected);
    answer_len = strlen(answer);

    for(i = 0U; active_charset[i] != '\0'; i++) {
        uint8_t idx;
        if(!morse_flipper_progress_char_idx(active_charset[i], &idx)) continue;
        progress->weak_seen[idx] = (uint8_t)(((uint16_t)progress->weak_seen[idx] * 15U) / 16U);
        progress->weak_error[idx] = (uint8_t)(((uint16_t)progress->weak_error[idx] * 15U) / 16U);
    }

    for(i = 0U; i < expected_len; i++) {
        uint8_t expected_idx;
        char want = morse_flipper_progress_upper(expected[i]);
        char got = i < answer_len ? morse_flipper_progress_upper(answer[i]) : '\0';

        if(morse_flipper_progress_char_idx(want, &expected_idx)) {
            progress->weak_seen[expected_idx] =
                morse_flipper_progress_sat_add8(progress->weak_seen[expected_idx], 16U);
            if(got == '\0' || got != want) {
                progress->weak_error[expected_idx] =
                    morse_flipper_progress_sat_add8(progress->weak_error[expected_idx], 32U);
            }
        }

        if(got != '\0' && got != want &&
           morse_flipper_progress_charset_contains(active_charset, got)) {
            uint8_t answer_idx;
            if(morse_flipper_progress_char_idx(got, &answer_idx)) {
                progress->weak_seen[answer_idx] =
                    morse_flipper_progress_sat_add8(progress->weak_seen[answer_idx], 8U);
                progress->weak_error[answer_idx] =
                    morse_flipper_progress_sat_add8(progress->weak_error[answer_idx], 16U);
            }
        }
    }

    for(i = expected_len; i < answer_len; i++) {
        uint8_t answer_idx;
        char got = morse_flipper_progress_upper(answer[i]);

        if(!morse_flipper_progress_charset_contains(active_charset, got)) continue;
        if(!morse_flipper_progress_char_idx(got, &answer_idx)) continue;
        progress->weak_seen[answer_idx] =
            morse_flipper_progress_sat_add8(progress->weak_seen[answer_idx], 8U);
        progress->weak_error[answer_idx] =
            morse_flipper_progress_sat_add8(progress->weak_error[answer_idx], 16U);
    }
}

uint8_t morse_flipper_progress_top_weak(
    const MorseFlipperProgress* progress,
    const char* active_charset,
    char* out,
    uint8_t out_cap) {
    char top_ch[3] = {0};
    uint16_t top_score[3] = {0};
    uint8_t top_error[3] = {0};
    uint8_t count = 0U;
    uint8_t i;

    if(out != NULL && out_cap != 0U) out[0] = '\0';
    if(progress == NULL || active_charset == NULL || out == NULL || out_cap == 0U) return 0U;

    for(i = 0U; active_charset[i] != '\0'; i++) {
        uint8_t idx;
        uint8_t seen;
        uint8_t error;
        uint16_t denom;
        uint16_t score;
        uint8_t slot;
        bool duplicate = false;
        uint8_t j;

        if(!morse_flipper_progress_char_idx(active_charset[i], &idx)) continue;
        for(j = 0U; j < i; j++) {
            uint8_t prev;
            if(morse_flipper_progress_char_idx(active_charset[j], &prev) && prev == idx) {
                duplicate = true;
                break;
            }
        }
        if(duplicate) continue;

        seen = progress->weak_seen[idx];
        error = progress->weak_error[idx];
        if(error < 16U) continue;

        denom = seen < 16U ? 16U : seen;
        score = (uint16_t)(((uint16_t)error * 255U) / denom);
        for(slot = 0U; slot < 3U; slot++) {
            if(top_ch[slot] == 0 || score > top_score[slot] ||
               (score == top_score[slot] && error > top_error[slot])) {
                uint8_t move;
                for(move = 2U; move > slot; move--) {
                    top_ch[move] = top_ch[move - 1U];
                    top_score[move] = top_score[move - 1U];
                    top_error[move] = top_error[move - 1U];
                }
                top_ch[slot] = (char)idx;
                top_score[slot] = score;
                top_error[slot] = error;
                break;
            }
        }
    }

    for(i = 0U; i < 3U && top_ch[i] != 0; i++) {
        size_t len = strlen(out);
        if(i != 0U) {
            if(len + 3U >= out_cap) break;
            out[len++] = ',';
            out[len++] = ' ';
            out[len] = '\0';
        }
        if(len + 2U >= out_cap) break;
        out[len++] = top_ch[i];
        out[len] = '\0';
        count++;
    }

    if(count == 0U && out_cap >= 2U) {
        out[0] = '-';
        out[1] = '\0';
    }

    return count;
}

bool morse_flipper_progress_date_to_day(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint16_t* out_day) {
    uint32_t total = 0U;
    uint16_t y;
    uint8_t m;
    uint8_t mdays;

    if(out_day == NULL) return false;
    *out_day = MORSE_FLIPPER_PROGRESS_DAY_NONE;
    if(year < MORSE_FLIPPER_PROGRESS_EPOCH_YEAR || year > MORSE_FLIPPER_PROGRESS_MAX_YEAR)
        return false;
    mdays = morse_flipper_progress_month_days(year, month);
    if(mdays == 0U || day < 1U || day > mdays) return false;

    for(y = MORSE_FLIPPER_PROGRESS_EPOCH_YEAR; y < year; y++)
        total += morse_flipper_progress_leap(y) ? 366U : 365U;
    for(m = 1U; m < month; m++)
        total += morse_flipper_progress_month_days(year, m);
    total += (uint32_t)day - 1U;
    if(total >= MORSE_FLIPPER_PROGRESS_DAY_NONE) return false;

    *out_day = (uint16_t)total;
    return true;
}

bool morse_flipper_progress_day_to_date(
    uint16_t practice_day,
    uint16_t* out_year,
    uint8_t* out_month,
    uint8_t* out_day) {
    uint16_t year = MORSE_FLIPPER_PROGRESS_EPOCH_YEAR;
    uint16_t day = practice_day;
    uint16_t ydays;
    uint8_t month = 1U;
    uint8_t mdays;

    if(out_year == NULL || out_month == NULL || out_day == NULL) return false;
    if(practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE) return false;

    while(year <= MORSE_FLIPPER_PROGRESS_MAX_YEAR) {
        ydays = morse_flipper_progress_leap(year) ? 366U : 365U;
        if(day < ydays) break;
        day = (uint16_t)(day - ydays);
        year++;
    }
    if(year > MORSE_FLIPPER_PROGRESS_MAX_YEAR) return false;

    while(month <= 12U) {
        mdays = morse_flipper_progress_month_days(year, month);
        if(day < mdays) break;
        day = (uint16_t)(day - mdays);
        month++;
    }
    if(month > 12U) return false;

    *out_year = year;
    *out_month = month;
    *out_day = (uint8_t)(day + 1U);
    return true;
}

bool morse_flipper_progress_history_path(char* out, size_t out_sz, uint16_t practice_day) {
    uint16_t year;
    uint8_t month;
    uint8_t day;

    if(out == NULL || out_sz == 0U) return false;
    out[0] = '\0';
    if(!morse_flipper_progress_day_to_date(practice_day, &year, &month, &day)) return false;

    return snprintf(
               out,
               out_sz,
               "%s/%04u%02u/%04u%02u%02u.txt",
               MORSE_FLIPPER_PROGRESS_HISTORY_DIR,
               (unsigned)year,
               (unsigned)month,
               (unsigned)year,
               (unsigned)month,
               (unsigned)day) < (int)out_sz;
}

bool morse_flipper_progress_history_row_date(
    const MorseFlipperProgressHistoryRow* row,
    uint16_t* out_year,
    uint8_t* out_month,
    uint8_t* out_day) {
    if(row == NULL) return false;
    return morse_flipper_progress_day_to_date(row->practice_day, out_year, out_month, out_day);
}

void morse_flipper_progress_history_date_label(
    const MorseFlipperProgressHistoryRow* row,
    uint16_t reference_year,
    uint8_t reference_month,
    char* out,
    size_t out_sz) {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    bool recent = true;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    if(!morse_flipper_progress_history_row_date(row, &year, &month, &day) || month < 1U ||
       month > 12U) {
        return;
    }

    if(reference_year > MORSE_FLIPPER_PROGRESS_EPOCH_YEAR && reference_month >= 1U &&
       reference_month <= 12U) {
        uint16_t cutoff_year = (uint16_t)(reference_year - 1U);
        recent = year > cutoff_year || (year == cutoff_year && month > reference_month);
    }

    if(recent) {
        snprintf(out, out_sz, "%02u %s", (unsigned)day, morse_flipper_progress_months[month - 1U]);
    } else {
        snprintf(
            out,
            out_sz,
            "%s %02u",
            morse_flipper_progress_months[month - 1U],
            (unsigned)(year % 100U));
    }
}

uint16_t morse_flipper_progress_history_start_day(
    const MorseFlipperProgress* progress,
    bool today_valid,
    uint16_t today) {
    if(progress != NULL && morse_flipper_progress_valid(progress) &&
       progress->last_stats_day != MORSE_FLIPPER_PROGRESS_DAY_NONE) {
        return progress->last_stats_day;
    }

    return today_valid ? today : MORSE_FLIPPER_PROGRESS_DAY_NONE;
}

static bool morse_flipper_progress_parse_history_line(
    const char* line,
    uint16_t practice_day,
    uint16_t line_from_end,
    MorseFlipperProgressHistoryRow* out) {
    uint8_t hour;
    uint8_t minute;
    uint8_t lesson;
    uint8_t max_lesson;
    uint8_t percent;

    if(line == NULL || out == NULL) return false;
    if(!morse_flipper_progress_digit(line[0]) || !morse_flipper_progress_digit(line[1]) ||
       !morse_flipper_progress_digit(line[2]) || !morse_flipper_progress_digit(line[3]) ||
       line[4] != ' ' || !morse_flipper_progress_digit(line[5]) ||
       !morse_flipper_progress_digit(line[6]) || line[7] != ' ' ||
       !morse_flipper_progress_digit(line[8]) || !morse_flipper_progress_digit(line[9]) ||
       !morse_flipper_progress_digit(line[10]) || line[11] != '\n')
        return false;

    hour = (uint8_t)(((line[0] - '0') * 10) + (line[1] - '0'));
    minute = (uint8_t)(((line[2] - '0') * 10) + (line[3] - '0'));
    lesson = (uint8_t)(((line[5] - '0') * 10) + (line[6] - '0'));
    percent = (uint8_t)(((line[8] - '0') * 100) + ((line[9] - '0') * 10) + (line[10] - '0'));
    max_lesson = (uint8_t)morse_trainer_lesson_count();
    if(hour > 23U || minute > 59U || lesson < 1U || lesson > max_lesson || percent > 100U)
        return false;

    *out = (MorseFlipperProgressHistoryRow){
        .lesson = morse_flipper_progress_lesson_letter(lesson),
        .practice_day = practice_day,
        .line_from_end = line_from_end,
        .hour = hour,
        .minute = minute,
        .lesson_idx = lesson,
        .percent = percent,
    };
    return true;
}

static uint32_t morse_flipper_progress_history_line_count(uint16_t practice_day) {
    char path[96];
    uint32_t total_lines = 0U;

    if(!morse_flipper_progress_history_path(path, sizeof(path), practice_day)) return 0U;

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        total_lines =
            (uint32_t)(storage_file_size(file) / MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
#else
    FILE* f = fopen(path, "rb");
    if(f != NULL) {
        if(fseek(f, 0L, SEEK_END) == 0) {
            long size = ftell(f);
            if(size > 0L) total_lines = (uint32_t)(size / MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN);
        }
        fclose(f);
    }
#endif

    return total_lines;
}

static bool morse_flipper_progress_history_load_row_at(
    uint16_t practice_day,
    uint16_t line_from_end,
    MorseFlipperProgressHistoryRow* out) {
    char path[96];
    uint32_t total_lines;
    uint32_t row_idx;
    uint32_t offset;
    char line[MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN];
    bool got_line = false;

    if(out == NULL) return false;
    if(!morse_flipper_progress_history_path(path, sizeof(path), practice_day)) return false;

    total_lines = morse_flipper_progress_history_line_count(practice_day);
    if(total_lines == 0U || (uint32_t)line_from_end >= total_lines) return false;

    row_idx = total_lines - 1U - line_from_end;
    offset = row_idx * MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN;

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_seek(file, (int32_t)offset, true) &&
           storage_file_read(file, line, sizeof(line)) == sizeof(line)) {
            got_line = true;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
#else
    FILE* f = fopen(path, "rb");
    if(f != NULL) {
        if(fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(line, 1U, sizeof(line), f) == sizeof(line)) {
            got_line = true;
        }
        fclose(f);
    }
#endif

    return got_line &&
           morse_flipper_progress_parse_history_line(line, practice_day, line_from_end, out);
}

#ifndef MORSE_FLIPPER_FAP
static void morse_flipper_progress_host_mkdirs(void) {
    mkdir("ext", 0777);
    mkdir("ext/apps_data", 0777);
    mkdir(MORSE_FLIPPER_APP_DATA_DIR, 0777);
    mkdir(MORSE_FLIPPER_PROGRESS_HISTORY_DIR, 0777);
}
#endif

bool morse_flipper_progress_load(MorseFlipperProgress* progress) {
    if(progress == NULL) return false;
    morse_flipper_progress_reset(progress);

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    FileInfo info = {0};
    FS_Error stat_err;
    uint16_t got = 0U;
    bool ok = true;

    stat_err = storage_common_stat(storage, MORSE_FLIPPER_PROGRESS_PATH, &info);
    if(stat_err == FSE_NOT_EXIST) {
        ok = true;
    } else if(stat_err != FSE_OK) {
        ok = false;
    } else if(file_info_is_dir(&info)) {
        morse_flipper_progress_reset(progress);
    } else if(info.size == MORSE_FLIPPER_PROGRESS_SIZE) {
        if(storage_file_open(file, MORSE_FLIPPER_PROGRESS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            got = storage_file_read(file, progress, sizeof(*progress));
            if(got != sizeof(*progress)) ok = false;
            if(ok && !morse_flipper_progress_valid(progress))
                morse_flipper_progress_reset(progress);
        } else {
            ok = false;
        }
    } else {
        morse_flipper_progress_reset(progress);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!ok) morse_flipper_progress_reset(progress);
    return ok;
#else
    FILE* f = fopen(MORSE_FLIPPER_PROGRESS_PATH, "rb");
    long size;
    size_t got;

    if(f == NULL) return errno == ENOENT;
    if(fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    size = ftell(f);
    rewind(f);
    if(size == (long)MORSE_FLIPPER_PROGRESS_SIZE) {
        got = fread(progress, 1U, sizeof(*progress), f);
        fclose(f);
        if(got != sizeof(*progress)) {
            morse_flipper_progress_reset(progress);
            return false;
        }
        if(!morse_flipper_progress_valid(progress)) morse_flipper_progress_reset(progress);
        return true;
    }

    fclose(f);
    morse_flipper_progress_reset(progress);
    return true;
#endif
}

bool morse_flipper_progress_save(const MorseFlipperProgress* progress) {
    if(!morse_flipper_progress_valid(progress)) return false;

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint16_t wrote = 0U;

    storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
    if(storage_file_open(file, MORSE_FLIPPER_PROGRESS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        wrote = storage_file_write(file, progress, sizeof(*progress));
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return wrote == sizeof(*progress);
#else
    FILE* f;
    size_t wrote;

    morse_flipper_progress_host_mkdirs();
    f = fopen(MORSE_FLIPPER_PROGRESS_PATH, "wb");
    if(f == NULL) return false;
    wrote = fwrite(progress, 1U, sizeof(*progress), f);
    fclose(f);
    return wrote == sizeof(*progress);
#endif
}

bool morse_flipper_progress_append_history(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t lesson,
    uint8_t percent) {
    uint16_t practice_day;
    char path[96];
    char month_dir[80];
    char line[MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN + 1U];

    if(!morse_flipper_progress_date_to_day(year, month, day, &practice_day)) return false;
    if(!morse_flipper_progress_history_path(path, sizeof(path), practice_day)) return false;
    if(hour > 23U || minute > 59U) return false;
    lesson = morse_flipper_progress_clamp_lesson(lesson);
    if(percent > 100U) percent = 100U;

    snprintf(
        month_dir,
        sizeof(month_dir),
        "%s/%04u%02u",
        MORSE_FLIPPER_PROGRESS_HISTORY_DIR,
        (unsigned)year,
        (unsigned)month);
    line[0] = (char)('0' + (hour / 10U));
    line[1] = (char)('0' + (hour % 10U));
    line[2] = (char)('0' + (minute / 10U));
    line[3] = (char)('0' + (minute % 10U));
    line[4] = ' ';
    line[5] = (char)('0' + (lesson / 10U));
    line[6] = (char)('0' + (lesson % 10U));
    line[7] = ' ';
    line[8] = (char)('0' + (percent / 100U));
    line[9] = (char)('0' + ((percent / 10U) % 10U));
    line[10] = (char)('0' + (percent % 10U));
    line[11] = '\n';
    line[12] = '\0';

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint16_t wrote = 0U;

    storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
    storage_common_mkdir(storage, MORSE_FLIPPER_PROGRESS_HISTORY_DIR);
    storage_common_mkdir(storage, month_dir);
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND))
        wrote = storage_file_write(file, line, MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return wrote == MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN;
#else
    FILE* f;
    size_t wrote;

    morse_flipper_progress_host_mkdirs();
    mkdir(month_dir, 0777);
    f = fopen(path, "ab");
    if(f == NULL) return false;
    wrote = fwrite(line, 1U, MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN, f);
    fclose(f);
    return wrote == MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN;
#endif
}

void morse_flipper_progress_history_reset(
    MorseFlipperProgressHistoryCursor* cursor,
    uint16_t practice_day) {
    if(cursor == NULL) return;

    cursor->practice_day = practice_day;
    cursor->line_from_end = 0U;
    cursor->exhausted = practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE;
}

static bool morse_flipper_progress_history_prev_day(MorseFlipperProgressHistoryCursor* cursor) {
    if(cursor == NULL || cursor->practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE ||
       cursor->practice_day == 0U) {
        if(cursor != NULL) cursor->exhausted = true;
        return false;
    }

    cursor->practice_day--;
    cursor->line_from_end = 0U;
    return true;
}

uint8_t morse_flipper_progress_history_load_more(
    MorseFlipperProgressHistoryCursor* cursor,
    MorseFlipperProgressHistoryRow* rows,
    uint8_t row_cap) {
    uint8_t count = 0U;
    uint8_t probed_days = 0U;

    if(cursor == NULL || rows == NULL || row_cap == 0U || cursor->exhausted) return 0U;

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
#endif

    while(count < row_cap && !cursor->exhausted &&
          probed_days < MORSE_FLIPPER_PROGRESS_HISTORY_SCAN_DAYS) {
        char path[96];
        bool opened = false;
        uint32_t total_lines = 0U;

        if(!morse_flipper_progress_history_path(path, sizeof(path), cursor->practice_day)) {
            cursor->exhausted = true;
            break;
        }

#ifdef MORSE_FLIPPER_FAP
        FileInfo info = {0};
        if(storage_common_stat(storage, path, &info) == FSE_OK && !file_info_is_dir(&info)) {
            opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
            if(opened) {
                total_lines =
                    (uint32_t)(storage_file_size(file) / MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN);
            }
        }
#else
        FILE* f = fopen(path, "rb");
        if(f != NULL) {
            long size = 0L;
            if(fseek(f, 0L, SEEK_END) == 0) {
                size = ftell(f);
                if(size > 0L)
                    total_lines = (uint32_t)(size / MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN);
            }
            opened = true;
        }
#endif

        while(opened && count < row_cap && cursor->line_from_end < total_lines) {
            char line[MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN];
            uint16_t line_from_end = cursor->line_from_end;
            uint32_t row_idx = total_lines - 1U - cursor->line_from_end;
            uint32_t offset = row_idx * MORSE_FLIPPER_PROGRESS_HISTORY_LINE_LEN;
            bool got_line = false;

#ifdef MORSE_FLIPPER_FAP
            if(storage_file_seek(file, (int32_t)offset, true) &&
               storage_file_read(file, line, sizeof(line)) == sizeof(line)) {
                got_line = true;
            }
#else
            if(fseek(f, (long)offset, SEEK_SET) == 0 &&
               fread(line, 1U, sizeof(line), f) == sizeof(line)) {
                got_line = true;
            }
#endif
            cursor->line_from_end++;
            if(got_line && morse_flipper_progress_parse_history_line(
                               line, cursor->practice_day, line_from_end, &rows[count])) {
                count++;
            }
        }

#ifdef MORSE_FLIPPER_FAP
        if(opened) storage_file_close(file);
#else
        if(opened) fclose(f);
#endif

        if(opened && cursor->line_from_end < total_lines) break;
        probed_days++;
        if(!morse_flipper_progress_history_prev_day(cursor)) break;
    }

#ifdef MORSE_FLIPPER_FAP
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
#endif

    if(count == 0U && probed_days >= MORSE_FLIPPER_PROGRESS_HISTORY_SCAN_DAYS)
        cursor->exhausted = true;

    return count;
}

bool morse_flipper_progress_history_load_newer(
    const MorseFlipperProgressHistoryRow* from,
    uint16_t newest_day,
    MorseFlipperProgressHistoryRow* out) {
    uint16_t line;
    uint16_t day;
    uint8_t probed_days = 0U;

    if(from == NULL || out == NULL) return false;
    if(from->practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE ||
       newest_day == MORSE_FLIPPER_PROGRESS_DAY_NONE)
        return false;
    if(from->practice_day > newest_day) return false;

    line = from->line_from_end;
    while(line > 0U) {
        line--;
        if(morse_flipper_progress_history_load_row_at(from->practice_day, line, out)) return true;
    }

    day = (uint16_t)(from->practice_day + 1U);
    while(day <= newest_day && probed_days < MORSE_FLIPPER_PROGRESS_HISTORY_SCAN_DAYS) {
        uint32_t total_lines = morse_flipper_progress_history_line_count(day);

        if(total_lines != 0U) {
            uint32_t candidate = total_lines - 1U;
            if(candidate > UINT16_MAX) candidate = UINT16_MAX;

            while(true) {
                if(morse_flipper_progress_history_load_row_at(day, (uint16_t)candidate, out))
                    return true;
                if(candidate == 0U) break;
                candidate--;
            }
        }

        day++;
        probed_days++;
    }

    return false;
}

#ifdef MORSE_FLIPPER_FAP
bool morse_flipper_ensure_session_progress_loaded(MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->session_progress != NULL) return true;

    app->session_progress = malloc(sizeof(MorseFlipperProgress));
    if(app->session_progress == NULL) return false;
    if(!morse_flipper_progress_load(app->session_progress)) {
        free(app->session_progress);
        app->session_progress = NULL;
        return false;
    }
    return true;
}

bool morse_flipper_ensure_view_progress_loaded(MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->view_progress != NULL) return true;

    app->view_progress = malloc(sizeof(MorseFlipperProgress));
    if(app->view_progress == NULL) return false;
    if(!morse_flipper_progress_load(app->view_progress)) {
        free(app->view_progress);
        app->view_progress = NULL;
        return false;
    }
    return true;
}

void morse_flipper_release_session_progress(MorseFlipperApp* app, bool save) {
    if(app == NULL || app->session_progress == NULL) return;

    if(save) {
        morse_flipper_progress_save(app->session_progress);
    }
    free(app->session_progress);
    app->session_progress = NULL;
    app->session_progress_dirty = false;
}

void morse_flipper_release_view_progress(MorseFlipperApp* app) {
    if(app == NULL || app->view_progress == NULL) return;

    free(app->view_progress);
    app->view_progress = NULL;
}
#endif
