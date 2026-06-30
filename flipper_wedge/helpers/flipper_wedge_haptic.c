#include "flipper_wedge_haptic.h"
#include "../flipper_wedge.h"

void flipper_wedge_play_happy_bump(void* context) {
    FlipperWedge* app = context;

    // Get vibration duration based on setting
    uint32_t duration_ms;
    switch(app->vibration_level) {
        case FlipperWedgeVibrationOff:
            return;  // No vibration
        case FlipperWedgeVibrationLow:
            duration_ms = 30;
            break;
        case FlipperWedgeVibrationMedium:
            duration_ms = 60;
            break;
        case FlipperWedgeVibrationHigh:
            duration_ms = 100;
            break;
        default:
            duration_ms = 30;  // Default to low
            break;
    }

    notification_message(app->notification, &sequence_set_vibro_on);
    furi_thread_flags_wait(0, FuriFlagWaitAny, duration_ms);
    notification_message(app->notification, &sequence_reset_vibro);
}

void flipper_wedge_play_bad_bump(void* context) {
    FlipperWedge* app = context;
    notification_message(app->notification, &sequence_set_vibro_on);
    furi_thread_flags_wait(0, FuriFlagWaitAny, 100);
    notification_message(app->notification, &sequence_reset_vibro);
}

void flipper_wedge_play_long_bump(void* context) {
    FlipperWedge* app = context;
    for(int i = 0; i < 4; i++) {
        notification_message(app->notification, &sequence_set_vibro_on);
        furi_thread_flags_wait(0, FuriFlagWaitAny, 50);
        notification_message(app->notification, &sequence_reset_vibro);
        furi_thread_flags_wait(0, FuriFlagWaitAny, 100);
    }
}
