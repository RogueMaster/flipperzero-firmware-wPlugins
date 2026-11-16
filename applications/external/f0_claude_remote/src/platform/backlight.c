#include "backlight.h"

#include <furi.h>
#include <notification/notification_messages.h>

struct Backlight {
    NotificationApp* notification;
    /* The lock is a counter (notification_app.c:229-247): taken twice it keeps the desktop lit. */
    DisplayLight light;
};

Backlight* backlight_alloc(DisplayLight light) {
    Backlight* backlight = malloc(sizeof(Backlight));
    backlight->notification = furi_record_open(RECORD_NOTIFICATION);
    backlight->light = DisplayLightAuto;
    backlight_set(backlight, light);
    return backlight;
}

void backlight_free(Backlight* backlight) {
    backlight_set(backlight, DisplayLightAuto);
    furi_record_close(RECORD_NOTIFICATION);
    free(backlight);
}

static void backlight_leave(Backlight* backlight) {
    switch(backlight->light) {
    case DisplayLightOn:
        notification_message(backlight->notification, &sequence_display_backlight_enforce_auto);
        break;
    case DisplayLightAuto:
    case DisplayLightOff:
        break;
    }
}

/* Going dark forces it off: a press's own Backlight Time (notification_app.c:507-511) would
 * otherwise outlive the unlock. */
static void backlight_enter(Backlight* backlight) {
    switch(backlight->light) {
    case DisplayLightOn:
        notification_message(backlight->notification, &sequence_display_backlight_enforce_on);
        break;
    case DisplayLightOff:
        notification_message(backlight->notification, &sequence_display_backlight_off);
        break;
    case DisplayLightAuto:
        break;
    }
}

void backlight_set(Backlight* backlight, DisplayLight light) {
    if(light == backlight->light) {
        return;
    }
    backlight_leave(backlight);
    backlight->light = light;
    backlight_enter(backlight);
}
