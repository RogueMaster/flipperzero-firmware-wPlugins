// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Player display calibration shared by all songs: 1x at 240 BPM.
 * This is an application choice, not a TJA format default. */
#define TAIKO_SCROLL_REFERENCE_BPM 240.0f
/* Independent of display calibration: automatic hits never exceed 32 Hz. */
#define TAIKO_ROLL_MAX_HZ          32

typedef enum {
    TjaCourseEasy,
    TjaCourseNormal,
    TjaCourseHard,
    TjaCourseOni,
    TjaCourseEdit,
    TjaCourseTower,
    TjaCourseDan,
    TjaCourseCount,
} TjaCourse;

typedef struct {
    int32_t time_ms; /* relative to the beginning of the audio */
    uint8_t kind;
    /* BPM / TAIKO_SCROLL_REFERENCE_BPM * #SCROLL. Zero means 1 for manually built notes. */
    float scroll_speed;
} TjaNote;

typedef struct {
    int32_t time_ms;
    float bpm;
} TjaBpmChange;

typedef struct {
    int32_t time_ms;
    bool active;
} TjaGogoChange;

/* Every comma-delimited measure, including empty and hidden measures.
 * Zero speed hides the line without removing its seek destination. */
typedef struct {
    int32_t time_ms;
    float scroll_speed;
} TjaBar;

typedef struct {
    int32_t time_ms;
    int32_t end_ms; /* Note 8, exclusive. */
    float scroll_speed;
    uint16_t balloon_count; /* Zero for drumrolls. */
    uint8_t kind; /* 5/6: small/big roll; 7/9: balloon. */
    size_t first_run;
    size_t run_count;
    uint32_t hit_count;
} TjaRoll;

/* Compact runs avoid allocating a note for every automatic 32nd-note hit. */
typedef struct {
    double first_ms;
    double interval_ms;
    uint32_t count;
} TjaHitRun;

typedef struct {
    char title[128];
    char wave[256];
    char course[24];
    TjaCourse course_id;
    TjaNote* notes;
    size_t note_count;
    float initial_bpm;
    float min_note_speed; /* Conservative lower bound on absolute note speed. */
    TjaBpmChange* bpm_changes;
    size_t bpm_change_count;
    TjaRoll* rolls;
    size_t roll_count;
    TjaHitRun* hit_runs;
    size_t hit_run_count;
    TjaGogoChange* gogo_changes;
    size_t gogo_change_count;
    TjaBar* bars;
    size_t bar_count;
    float min_bar_speed;
    uint8_t roll_speed_tenths;
    int32_t end_ms;
} TjaChart;

/* Return 1 for a line (including an empty one), 0 for EOF, -1 for an error.
 * The reader must reject lines that do not fit, and NUL-terminate each line. */
typedef int (*TjaReadLine)(void* context, char* line, size_t capacity);

/* Reads only the first #START ... #END chart. On failure, chart is still safe to free. */
bool tja_parse(TjaReadLine reader, void* context, TjaChart* chart, const char** error);
bool tja_parse_course(
    TjaReadLine reader,
    void* context,
    TjaCourse course,
    TjaChart* chart,
    const char** error);
/* Scan available COURSE sections without loading note arrays. Rewind/reopen the
 * reader before parsing. Numeric COURSE names and Ura (Edit) are accepted. */
bool tja_list_courses(TjaReadLine reader, void* context, uint8_t* courses, const char** error);
const char* tja_course_name(TjaCourse course);
TjaCourse tja_default_course(uint8_t courses);
/* Original chart BPM at an audio position, including changes before time zero. */
float tja_chart_bpm_at(const TjaChart* chart, uint32_t position_ms);
/* GOGO is initially off and always ends at #END, including after a seek. */
bool tja_chart_gogo_at(const TjaChart* chart, uint32_t position_ms);
/* Strictly previous/next measure start, repeated count times. Clamp to audio
 * bounds; backward from inside a measure first returns to that measure's start. */
uint32_t tja_chart_seek_bars(
    const TjaChart* chart,
    uint32_t position_ms,
    uint32_t duration_ms,
    bool forward,
    uint32_t count);
/* Rebuild automatic hits for the playback speed, preserving a 32 Hz real-time
 * cap. Audio must be stopped/finished. Failure leaves the chart unchanged. */
bool tja_chart_set_roll_speed(TjaChart* chart, uint8_t speed_tenths, const char** error);
void tja_chart_free(TjaChart* chart);
