/*
 * alarm.h — drop alert. Plays a ~1 s two-tone siren on the Flipper speaker.
 * No GUI dependency, so it is callable from the headless background service.
 */
#pragma once

#include <notification/notification.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Blocks for ~1 second playing a siren, then stops and releases the speaker.
 * `notif` is optional: if non-NULL, the red LED is flashed alongside the siren.
 * Pass NULL from the headless service (speaker only).
 */
void alarm_play_siren(NotificationApp* notif);

#ifdef __cplusplus
}
#endif
