/* Plays a feedback cue on the LED, vibro and speaker (feedback.h has the
 * mapping). Flipper only. */
#pragma once

#include <notification/notification.h>

#include "app_settings.h"
#include "app_state.h"

void feedback_play(NotificationApp* notifications, AppCue cue, const AppSettings* settings);
