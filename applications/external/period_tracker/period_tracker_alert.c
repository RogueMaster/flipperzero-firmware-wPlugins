#include "period_tracker_alert.h"
#include "period_tracker_predictions.h"
#include "period_tracker_csv.h"
#include <furi_hal.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG                   "PeriodTrackerAlert"
#define MAX_ALERT_PREDICTIONS 20

// NOTE: RTC alarm APIs (furi_hal_rtc_set_alarm, furi_hal_rtc_set_alarm_callback)
// are not available for external FAP apps due to security restrictions.
// They are only available for built-in SETTINGS/STARTUP apps.
//
// This implementation provides a simpler approach:
// - Manual check via "Daily Digest" in the app
// - Optional: Run the app at alarm time (user responsibility)
//
// For full alarm integration, this app would need to be compiled as a
// built-in app (apptype=FlipperAppType.SETTINGS) instead of EXTERNAL.

// Notification sequence for period alerts - simple 2 beeps, no LCD flash
const NotificationSequence sequence_period_alert = {
    &message_force_speaker_volume_setting_1f,
    &message_force_vibro_setting_on,

    &message_vibro_on,
    &message_note_c7,
    &message_delay_100,

    &message_vibro_off,
    &message_sound_off,
    &message_delay_100,

    &message_note_c5,
    &message_delay_100,

    &message_sound_off,

    NULL,
};

// Play notification when app is opened if there are alerts today
// NOTE: Caller should check if there are alerts before calling this
// (don't do redundant check here to avoid stack overflow from Prediction array)
void period_tracker_alert_notify_if_today(Storage* storage) {
    UNUSED(storage);
    FURI_LOG_I(TAG, "Playing alert notification");

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    // Play a simple success sequence instead of custom one (to avoid hangs)
    notification_message(notifications, &sequence_success);

    furi_record_close(RECORD_NOTIFICATION);

    FURI_LOG_I(TAG, "Notification completed");
}

// Initialize the alert system
void period_tracker_alert_init(Storage* storage) {
    FURI_LOG_I(TAG, "Alert system initialized (notifications on app open only)");
    UNUSED(storage);
    // Nothing to initialize - we just check and notify on app open
}

// Deinitialize the alert system
void period_tracker_alert_deinit(void) {
    FURI_LOG_I(TAG, "Alert system deinitialized");
    // Nothing to clean up
}

// Stop the alert (called when app is opened)
void period_tracker_alert_stop(void) {
    // Not needed in simplified version
}

// Check if there are alerts for today (for showing badge/notification)
bool period_tracker_has_alerts_today(Storage* storage) {
    // Allocate on heap - too large for stack (~3KB)
    Prediction* predictions = malloc(sizeof(Prediction) * MAX_ALERT_PREDICTIONS);
    if(!predictions) {
        FURI_LOG_E(TAG, "Failed to allocate memory for alert check");
        return false;
    }

    // Note: No shared buffer available here - this is a standalone function
    // Called outside of app context. Pass NULL to use fallback malloc.
    uint16_t count = get_today_predictions(storage, predictions, MAX_ALERT_PREDICTIONS, NULL, 0);

    free(predictions);
    return count > 0;
}
