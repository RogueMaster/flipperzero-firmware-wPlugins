#pragma once

#include <gui/view.h>
#include <notification/notification.h>

typedef struct ProgressView ProgressView;

typedef void (*ProgressViewCallback)(void* context);

ProgressView* progress_view_alloc(void);

void progress_view_free(ProgressView* instance);

View* progress_view_get_view(ProgressView* instance);

/** Called when the user presses Right to open the badge gallery. */
void progress_view_set_callback(
    ProgressView* instance,
    ProgressViewCallback callback,
    void* context);

void progress_view_configure(
    ProgressView* instance,
    uint32_t level,
    uint32_t xp,
    uint32_t labs_done,
    uint32_t labs_total,
    uint32_t streak,
    const char* level_title,
    NotificationApp* notifications,
    bool sound_enabled);
