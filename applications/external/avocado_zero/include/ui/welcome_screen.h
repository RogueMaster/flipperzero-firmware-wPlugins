/**
 * Presentation: first-run welcome widget.
 */
#pragma once

#include "include/platform/feedback_helper.h"
#include <gui/view.h>
#include <gui/view_dispatcher.h>

typedef struct WelcomeScreen WelcomeScreen;

typedef struct {
    ViewDispatcher *vd;
    uint32_t view_game_index;
    AvocadoFeedback *feedback;
} WelcomeScreenContext;

WelcomeScreen *welcome_screen_alloc(const WelcomeScreenContext *ctx);
void welcome_screen_free(WelcomeScreen *screen);
View *welcome_screen_get_view(WelcomeScreen *screen);
