#pragma once

#include <storage/storage.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the alert system
 *
 * Note: External FAP apps cannot use RTC alarms due to API restrictions.
 * This is a placeholder for future built-in app integration.
 *
 * @param storage Pointer to storage instance
 */
void period_tracker_alert_init(Storage* storage);

/** Deinitialize the alert system
 *
 * Cleanup function for alert system resources.
 */
void period_tracker_alert_deinit(void);

/** Stop the alert
 *
 * Placeholder function - not used in FAP version.
 */
void period_tracker_alert_stop(void);

/** Play notification if there are alerts today
 *
 * Checks for today's predictions and plays an alert sound if found.
 * Called when app is opened.
 *
 * @param storage Pointer to storage instance
 */
void period_tracker_alert_notify_if_today(Storage* storage);

/** Check if there are alerts for today
 *
 * @param storage Pointer to storage instance
 * @return true if there are predictions/alerts for today
 */
bool period_tracker_has_alerts_today(Storage* storage);

#ifdef __cplusplus
}
#endif
