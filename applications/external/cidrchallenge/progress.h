#pragma once

#include "quiz.h"

#include <stdbool.h>
#include <stdint.h>

#define XP_PER_LEVEL      100
#define XP_CORRECT        10
#define XP_FAST_BONUS     5
#define XP_STREAK_BONUS   25
#define XP_FAST_ANSWER_MS 5000
#define XP_STREAK_STEP    5

typedef struct {
    uint32_t xp;
    uint32_t questions;
    uint32_t correct;
    uint32_t best_streak;
    uint32_t fastest_ms;
    uint32_t sessions;
} Stats;

typedef struct {
    Difficulty difficulty;
    uint8_t exam_index;
    bool sound;
} Settings;

void stats_load(Stats* stats);
void stats_save(const Stats* stats);
void stats_reset(Stats* stats);

void settings_load(Settings* settings);
void settings_save(const Settings* settings);

uint32_t stats_level(uint32_t xp);
uint32_t stats_accuracy(const Stats* stats);
const char* level_title(uint32_t level);
const char* accuracy_rank(uint32_t accuracy);
