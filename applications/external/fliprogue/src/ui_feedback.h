#pragma once

#include "ui_internal.h"

void feedback_update_led(AppContext* app, bool force);
void feedback_reset_led(AppContext* app, bool force);
FeedbackBefore feedback_capture(AppContext* app);
void finish_action(AppContext* app, FeedbackBefore before, FrActionResult result);
