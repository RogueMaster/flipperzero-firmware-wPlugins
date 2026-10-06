#pragma once
#include <notification/notification_messages.h>
#include "alerts.h"

/**
 * Plays the alert signal through the notification app (honours the Flipper's global
 * vibro / sound / stealth settings). Every rule has its own melody, so the alert can be
 * told apart by ear; critical rules get the more insistent vibro pattern.
 */
void phr_signal_play(NotificationApp* n, AlertSignal signal, AlertRuleId rule);
bool phr_signal_is_critical(AlertRuleId rule);
