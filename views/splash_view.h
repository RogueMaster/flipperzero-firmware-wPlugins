#pragma once

#include <gui/view.h>
#include <stdbool.h>

/**
 * Boot intro: a ping going out, the mark, the name.
 *
 * Short on purpose. An intro long enough to be admired is an intro you have to
 * sit through every single launch.
 */
typedef struct SplashView SplashView;
typedef void (*SplashViewCallback)(void* context);

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* sv);
View* splash_view_get_view(SplashView* sv);

/** Advance one frame; fires the done callback when the intro has played out. */
void splash_view_tick(SplashView* sv);

/** Called when the intro finishes, or when the user skips it with any key. */
void splash_view_set_done_callback(SplashView* sv, SplashViewCallback cb, void* context);
