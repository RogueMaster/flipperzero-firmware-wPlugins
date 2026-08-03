#pragma once

#include "nfc_canary_i.h"

typedef struct Alert Alert;

Alert* alert_alloc(NotificationApp* notifications, AlerterSettings* settings);
void alert_free(Alert* alert);

/* Start alerting at the given tier. Idempotent -- calling again with the same
 * tier does nothing; calling with a higher tier escalates. */
void alert_start(Alert* alert, ThreatTier tier);

/* Stop all alerting and release the speaker. Safe to call when idle. */
void alert_stop(Alert* alert);

/* Fire a short self-test burst (settings audition). Blocks ~1s. */
void alert_test(Alert* alert);

const char* alert_pattern_name(AlertPattern p);
const char* alert_tone_name(AlertTone t);
const char* alert_tier_name(ThreatTier t);
