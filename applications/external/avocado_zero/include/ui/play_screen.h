/**
 * Presentation: main play view (game, game over, victory).
 */
#pragma once

#include "include/domain/avocado_state.h"
#include "include/platform/feedback_helper.h"
#include <gui/view.h>

typedef struct PlayScreen PlayScreen;

PlayScreen *play_screen_alloc(AvocadoData *data, AvocadoFeedback *feedback);
void play_screen_free(PlayScreen *screen);
View *play_screen_get_view(PlayScreen *screen);
