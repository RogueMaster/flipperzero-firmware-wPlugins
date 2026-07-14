#pragma once

#include <gui/view.h>
#include <notification/notification.h>

#include "../helpers/pocketlab_content.h"
#include "../helpers/pocketlab_i18n.h"

typedef struct LessonView LessonView;

typedef void (*LessonViewDoneCallback)(void* context);

LessonView* lesson_view_alloc(void);

void lesson_view_free(LessonView* instance);

View* lesson_view_get_view(LessonView* instance);

/** Set the callback invoked when the learner finishes the final reward step. */
void lesson_view_set_done_callback(
    LessonView* instance,
    LessonViewDoneCallback callback,
    void* context);

/** Load a lab and reset the view to its first step. */
void lesson_view_configure(
    LessonView* instance,
    const PocketLabLab* lab,
    bool already_completed,
    bool sound_enabled,
    NotificationApp* notifications);
