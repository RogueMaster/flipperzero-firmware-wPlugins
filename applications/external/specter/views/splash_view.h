#pragma once

#include <gui/view.h>
#include <stdbool.h>

typedef struct SplashView SplashView;
typedef void (*SplashViewCallback)(void* context);

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* v);
View* splash_view_get_view(SplashView* v);

/* Fired when the intro finishes on its own, or when any key skips it. */
void splash_view_set_done_callback(SplashView* v, SplashViewCallback cb, void* context);

/* Advance the animation one frame. Returns true once the intro is complete, so
 * the scene can move on without needing a second timer. */
bool splash_view_tick(SplashView* v);

/* Rewind to frame 0. The view outlives the scene, so a second visit would
 * otherwise start past the end of the animation and show a finished frame. */
void splash_view_reset(SplashView* v);
