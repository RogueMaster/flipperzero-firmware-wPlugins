#pragma once
#include <notification/notification_messages.h>
#include "alerts.h"

/**
 * Plays the alert signal through the notification app (honours the Flipper's global
 * vibro / sound / stealth settings). `critical` selects the more insistent pattern.
 */
void phr_signal_play(NotificationApp* n, AlertSignal signal, bool critical);
bool phr_signal_is_critical(AlertRuleId rule);
