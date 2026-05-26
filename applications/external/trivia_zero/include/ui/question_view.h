#pragma once

#include "include/infrastructure/pack_reader.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QVIEW_MAX_LINES 32u

typedef struct {
    const char *lines[QVIEW_MAX_LINES];
    uint8_t count;
} LineSlices;

/* Pure: greedy word wrap into NUL-terminated slices stored inside `buf`.
 * Slices stay valid for the lifetime of `buf`. */
void qview_wrap(const char *text, uint8_t max_cols, char *buf, size_t buf_size, LineSlices *out);

/* ---- Furi-bound (hardware-only) ---- */

typedef enum {
    QViewModeQuestion,
    QViewModeAnswer,
} QViewMode;

typedef enum {
    QViewActionReveal,
    QViewActionNext,
    QViewActionPrev,
    QViewActionScrollUp,
    QViewActionScrollDown,
    QViewActionMenu,
} QViewAction;

typedef struct QuestionView QuestionView;

QuestionView *question_view_alloc(void);
void question_view_free(QuestionView *qv);
struct View *question_view_get_view(QuestionView *qv);

void question_view_set_question(QuestionView *qv, const Question *q);
void question_view_set_mode(QuestionView *qv, QViewMode mode);

void question_view_set_callback(QuestionView *qv, void *ctx,
                                void (*on_action)(void *ctx, QViewAction action));
