#pragma once

#include <gui/view.h>

typedef struct LevelUpView LevelUpView;

typedef void (*LevelUpViewDoneCallback)(void* context);

LevelUpView* levelup_view_alloc(void);

void levelup_view_free(LevelUpView* instance);

View* levelup_view_get_view(LevelUpView* instance);

/** Called when the celebration finishes or the user skips it. */
void levelup_view_set_done_callback(
    LevelUpView* instance,
    LevelUpViewDoneCallback callback,
    void* context);

/** Set the header, subtitle and duration (frames), then (re)start the animation. */
void levelup_view_configure(
    LevelUpView* instance,
    const char* header,
    const char* title,
    uint16_t frames);
