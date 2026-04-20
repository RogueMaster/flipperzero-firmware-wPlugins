#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HF_HABIT_NAME_MAX 16
#define HF_HISTORY_DAYS 7
#define HF_DEFAULT_GOAL 66
#define HF_GOAL_MIN 1
#define HF_GOAL_MAX 365
#define HF_HABITS_MAX 12

typedef struct {
    char name[HF_HABIT_NAME_MAX];
    uint16_t streak;
    uint16_t max_streak;
    bool completed_today;
    uint8_t history[HF_HISTORY_DAYS];
    uint16_t goal_days;
    bool mastered;
} Habit;

void hf_habit_set_defaults(Habit *h, const char *name);

void hf_history_push_completed(Habit *h, bool day_was_completed);

void hf_habit_on_long_gap(Habit *h);

void hf_habit_toggle_today(Habit *h, bool *out_just_mastered);

uint8_t hf_habit_progress_pct(const Habit *h);

bool hf_habit_shows_medal(const Habit *h);

void hf_habit_reset_streak(Habit *h);

void hf_habit_bump_max_streak(Habit *h);

void hf_habit_apply_overnight(Habit *h, bool prev_day_done);
