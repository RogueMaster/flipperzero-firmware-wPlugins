#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

#include "../helpers/curriculum.h"
#include "../helpers/tone_player.h"

#define MAX_CHOICES 13

typedef enum {
    QuizPhaseAnswering,
    QuizPhaseFeedback,
    QuizPhaseTeach, /* introducing a new item, before the quiz starts */
} QuizPhase;

/* Everything the view needs to draw one frame. The scene owns the real state
 * and pushes a snapshot in; the view stays pure rendering. */
typedef struct {
    QuizPhase phase;
    uint8_t mode; /* picks which name table the ids belong to */

    uint8_t question; /* 1-based, for the progress bar */
    uint8_t total;
    uint8_t score;
    uint8_t hints_left;
    uint8_t streak;

    uint8_t choices[MAX_CHOICES]; /* answer ids offered */
    bool eliminated[MAX_CHOICES]; /* struck out by a hint */
    uint8_t choice_count;
    uint8_t selected;

    bool last_correct;
    uint8_t correct_id; /* revealed during feedback */
    uint8_t played_notes[MAX_SEQUENCE]; /* drawn on the keyboard */
    uint8_t played_count;

    bool show_hint;
    bool teach_last; /* teach screen: this is the final new item */
    bool challenge;
    uint8_t mistakes_left; /* challenge only */
    bool quit_armed; /* Back pressed once */
} QuizModel;

typedef struct QuizView QuizView;

QuizView* quiz_view_alloc(void);
void quiz_view_free(QuizView* quiz_view);
View* quiz_view_get_view(QuizView* quiz_view);

void quiz_view_set_callback(QuizView* quiz_view, void (*callback)(void*, uint32_t), void* context);
void quiz_view_update(QuizView* quiz_view, const QuizModel* model);

/* Shared with the teach screen so both draw the same keyboard. */
void piano_draw(Canvas* canvas, uint8_t y, const uint8_t* notes, uint8_t count);
