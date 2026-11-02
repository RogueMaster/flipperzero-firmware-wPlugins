#include "alarm.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_speaker.h>
#include <notification/notification_messages.h>

#define TAG "drop_alarm"

#define SIREN_TOTAL_MS             1000u /* ~1 s, per spec */
#define SIREN_STEP_MS              120u /* tone swap interval */
#define SIREN_LOW_HZ               700.0f
#define SIREN_HIGH_HZ              1400.0f
#define SIREN_VOLUME               1.0f /* loud */
#define SPEAKER_ACQUIRE_TIMEOUT_MS 1000u

void alarm_play_siren(NotificationApp* notif) {
    if(notif) notification_message(notif, &sequence_set_only_red_255);

    if(!furi_hal_speaker_acquire(SPEAKER_ACQUIRE_TIMEOUT_MS)) {
        FURI_LOG_W(TAG, "speaker busy, skipping siren");
        if(notif) notification_message(notif, &sequence_reset_red);
        return;
    }

    uint32_t elapsed = 0;
    bool high = false;
    while(elapsed < SIREN_TOTAL_MS) {
        furi_hal_speaker_start(high ? SIREN_HIGH_HZ : SIREN_LOW_HZ, SIREN_VOLUME);
        furi_delay_ms(SIREN_STEP_MS);
        high = !high;
        elapsed += SIREN_STEP_MS;
    }

    furi_hal_speaker_stop();
    furi_hal_speaker_release();

    if(notif) notification_message(notif, &sequence_reset_red);
}
