/**
 * Faraday - launch splash.
 *
 * A one-shot branded intro shown the first time the menu is reached: the pouch
 * with a fob inside it radiating waves that stay contained, the wordmark, the
 * tagline and a progress bar that fills as it auto-advances. Any key skips it.
 * Purely decorative - it holds no radio and blocks nothing.
 */
#pragma once

#include <gui/view.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SplashViewSkipCallback)(void* context);

typedef struct SplashView SplashView;

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* v);
View* splash_view_get_view(SplashView* v);

/** Fired when the user presses any key to skip the intro. */
void splash_view_set_skip_callback(SplashView* v, SplashViewSkipCallback cb, void* context);

/** 0..100, drives the progress bar. */
void splash_view_set_progress(SplashView* v, uint8_t progress);

/** Advance the wave animation. */
void splash_view_tick(SplashView* v);

#ifdef __cplusplus
}
#endif
