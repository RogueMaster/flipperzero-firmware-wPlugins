#pragma once

#include <gui/view.h>

typedef struct SplashView SplashView;
typedef void (*SplashViewCallback)(void* context);

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* v);
View* splash_view_get_view(SplashView* v);

/* Fired when the intro finishes on its own, or when any key skips it. */
void splash_view_set_done_callback(SplashView* v, SplashViewCallback cb, void* context);

/* Advance the animation one frame. Returns true once the intro is complete, so
 * the scene can move on without a second timer. */
bool splash_view_tick(SplashView* v);
