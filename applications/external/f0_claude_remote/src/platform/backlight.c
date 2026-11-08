#include "backlight.h"

#include <furi.h>
#include <notification/notification_messages.h>

struct Backlight {
    NotificationApp* notification;
    /* The lock is a counter (notification_app.c:229-247): taken twice it keeps the desktop lit. */
    bool lit;
};

Backlight* backlight_alloc(void) {
    Backlight* backlight = malloc(sizeof(Backlight));
    backlight->notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(backlight->notification, &sequence_display_backlight_enforce_on);
    backlight->lit = true;
    return backlight;
}

void backlight_free(Backlight* backlight) {
    if(backlight->lit) {
        notification_message(backlight->notification, &sequence_display_backlight_enforce_auto);
    }
    furi_record_close(RECORD_NOTIFICATION);
    free(backlight);
}

/* Going dark forces it off: a press's own Backlight Time (notification_app.c:507-511) would
 * otherwise outlive the unlock. */
void backlight_set(Backlight* backlight, bool lit) {
    if(lit == backlight->lit) {
        return;
    }
    backlight->lit = lit;
    if(lit) {
        notification_message(backlight->notification, &sequence_display_backlight_enforce_on);
    } else {
        notification_message(backlight->notification, &sequence_display_backlight_enforce_auto);
        notification_message(backlight->notification, &sequence_display_backlight_off);
    }
}
