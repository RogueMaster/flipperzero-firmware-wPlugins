#pragma once
#include <notification/notification.h> /* N9: include root is applications/services/ */
#include "sr_alert.h"
#include "sr_settings.h"

/* Dispatches by the settings' effective bits (stealth already folded in, N10).
   n == NULL or kind == SrAlertNone is a no-op. */
void sr_notify_alert(NotificationApp* n, SrAlertKind kind, const SrSettings* s);

/* Short click only; no vibro, no LED. n == NULL or s == NULL is a no-op.
 * effective_sound already folds stealth off. */
void sr_notify_newnet(NotificationApp* n, const SrSettings* s);

/* on = enforce backlight on; off = restore enforce_auto. n == NULL is a no-op. */
void sr_notify_backlight_enforce(NotificationApp* n, bool on);
