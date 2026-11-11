/**
 * Per-question study statistics and exam history for one pool, saved to the
 * SD card between sessions.
 *
 * Each question has one status byte:
 *   bits 0-2  review weight. 0 = not in the review list. A wrong answer adds
 *             2 (capped at 7), a right answer removes 1, so questions missed
 *             repeatedly stay in review longer and are shown first.
 *   bit 6     the most recent answer was correct ("mastered")
 *   bit 7     the question has been answered at least once
 */
#pragma once

#include "pool.h"

#define PROGRESS_WEIGHT_MASK  0x07
#define PROGRESS_WEIGHT_MAX   7
#define PROGRESS_WEIGHT_MISS  2
#define PROGRESS_LAST_CORRECT 0x40
#define PROGRESS_SEEN         0x80

typedef struct {
    uint16_t count;
    uint8_t* stats;
    uint16_t exams_taken;
    uint16_t exams_passed;
    uint8_t best_score;
    uint8_t last_score;
    bool dirty;
} Progress;

typedef struct {
    uint16_t total; /**< Questions in range (after the figure filter) */
    uint16_t seen;
    uint16_t mastered;
    uint16_t review; /**< Questions with a non-zero review weight */
} ProgressSummary;

Progress* progress_alloc(uint16_t count);
void progress_free(Progress* progress);
void progress_reset(Progress* progress);

/** Load saved progress. Entries are matched to questions by their stable
 * code, so errata that add or remove questions do not scramble the stats.
 * Returns false (leaving progress empty) if there is no usable save. */
bool progress_load(Progress* progress, Storage* storage, const char* path, const Pool* pool);
bool progress_save(Progress* progress, Storage* storage, const char* path, const Pool* pool);

void progress_record(Progress* progress, uint16_t question, bool correct);
void progress_record_exam(Progress* progress, uint8_t score, bool passed);

static inline uint8_t progress_weight(const Progress* progress, uint16_t question) {
    return progress->stats[question] & PROGRESS_WEIGHT_MASK;
}

static inline bool progress_mastered(const Progress* progress, uint16_t question) {
    return (progress->stats[question] & PROGRESS_LAST_CORRECT) != 0;
}

void progress_summarize(
    const Progress* progress,
    const Pool* pool,
    uint16_t first,
    uint16_t count,
    bool skip_figures,
    ProgressSummary* summary);
