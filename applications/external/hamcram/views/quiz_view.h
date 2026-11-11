/**
 * The question screen: a status bar, then the question and its four answers
 * as one word-wrapped, scrollable column.
 *
 * Up/Down first scroll through whatever is too long to fit, then step from
 * answer to answer. OK picks the highlighted answer. Left/Right move between
 * questions. Once the answer is revealed, OK also moves on.
 */
#pragma once

#include <gui/view.h>

typedef struct QuizView QuizView;

typedef enum {
    QuizViewEventAnswer, /**< `slot` is the displayed answer position, 0-3 */
    QuizViewEventNext,
    QuizViewEventPrev,
} QuizViewEvent;

typedef void (*QuizViewCallback)(QuizViewEvent event, uint8_t slot, void* context);

typedef struct {
    const char* left; /**< Status bar, left: position in the session */
    const char* center; /**< Status bar, middle: e.g. "Correct" */
    const char* right; /**< Status bar, right: question ID */
    const char* note; /**< Optional line shown before the question */
    const char* question;
    const char* answers[4]; /**< In display order */
    const char* footer; /**< Optional line after the answers, shown once revealed */
    int8_t chosen; /**< Displayed slot the user picked, or -1 */
    int8_t correct; /**< Displayed slot of the right answer, or -1 */
    bool reveal; /**< Show which answer is right; answering is disabled */
} QuizViewData;

QuizView* quiz_view_alloc(void);
void quiz_view_free(QuizView* quiz_view);
View* quiz_view_get_view(QuizView* quiz_view);
void quiz_view_set_callback(QuizView* quiz_view, QuizViewCallback callback, void* context);

/** Show a question, scrolled to the top */
void quiz_view_set_question(QuizView* quiz_view, const QuizViewData* data);

/** Reveal the answer to the question on screen without losing the scroll
 * position, bringing the right answer into view. */
void quiz_view_reveal(QuizView* quiz_view, int8_t chosen, int8_t correct, const char* center);
