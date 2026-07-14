#pragma once

#include <gui/view.h>
#include <notification/notification.h>

typedef struct ExamView ExamView;

typedef void (*ExamViewDoneCallback)(void* context);

ExamView* exam_view_alloc(void);

void exam_view_free(ExamView* instance);

View* exam_view_get_view(ExamView* instance);

/** Called when the exam is finished or exited. */
void exam_view_set_done_callback(ExamView* instance, ExamViewDoneCallback callback, void* context);

/** Number of correct answers in the finished exam (0 if none / not finished). */
uint8_t exam_view_get_score(ExamView* instance);

/** Build a random exam from the quizzes of the completed labs. */
void exam_view_configure(
    ExamView* instance,
    uint64_t completed_mask,
    bool sound_enabled,
    NotificationApp* notifications);
