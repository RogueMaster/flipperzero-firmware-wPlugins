#include "include/domain/habit.h"
#include <stdio.h>
#include <string.h>

void hf_habit_set_defaults(Habit *h, const char *name) {
    memset(h, 0, sizeof(*h));
    if (name) {
        snprintf(h->name, sizeof(h->name), "%s", name);
    }
    h->goal_days = HF_DEFAULT_GOAL;
}

void hf_history_push_completed(Habit *h, bool day_was_completed) {
    for (int i = 0; i < HF_HISTORY_DAYS - 1; i++) {
        h->history[i] = h->history[i + 1];
    }
    h->history[HF_HISTORY_DAYS - 1] = day_was_completed ? 1u : 0u;
}

void hf_habit_on_long_gap(Habit *h) {
    h->streak = 0;
    h->completed_today = false;
    memset(h->history, 0, sizeof(h->history));
}

void hf_habit_bump_max_streak(Habit *h) {
    if (h->streak > h->max_streak) {
        h->max_streak = h->streak;
    }
}

void hf_habit_toggle_today(Habit *h, bool *out_just_mastered) {
    if (out_just_mastered) {
        *out_just_mastered = false;
    }
    if (h->completed_today) {
        h->completed_today = false;
        if (h->streak > 0) {
            h->streak--;
        }
        return;
    }
    h->completed_today = true;
    if (h->streak < UINT16_MAX) {
        h->streak++;
    }
    hf_habit_bump_max_streak(h);
    if (!h->mastered && h->goal_days > 0 && h->streak >= h->goal_days) {
        h->mastered = true;
        if (out_just_mastered) {
            *out_just_mastered = true;
        }
    }
}

uint8_t hf_habit_progress_pct(const Habit *h) {
    if (h->goal_days == 0) {
        return 0;
    }
    const uint32_t num = (uint32_t)h->streak * 100u;
    uint32_t pct = num / (uint32_t)h->goal_days;
    if (pct > 100u) {
        pct = 100u;
    }
    return (uint8_t)pct;
}

bool hf_habit_shows_medal(const Habit *h) {
    return h->mastered || (h->goal_days > 0 && h->streak >= h->goal_days);
}

void hf_habit_reset_streak(Habit *h) {
    h->streak = 0;
    h->completed_today = false;
}

void hf_habit_apply_overnight(Habit *h, bool prev_day_done) {
    hf_history_push_completed(h, prev_day_done);
    h->completed_today = false;
    if (!prev_day_done) {
        h->streak = 0;
    }
}
