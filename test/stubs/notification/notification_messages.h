#pragma once
#include <furi.h>
#define RECORD_NOTIFICATION "notification"
typedef struct NotificationApp NotificationApp;
typedef enum {
    NotificationMessageTypeLedRed,
    NotificationMessageTypeLedGreen,
    NotificationMessageTypeLedBlue,
    NotificationMessageTypeDelay,
    NotificationMessageTypeDoNotReset,
} NotificationMessageType;
typedef struct {
    NotificationMessageType type;
    union {
        struct { uint8_t value; } led;
        struct { uint32_t length; } delay;
    } data;
} NotificationMessage;
typedef const NotificationMessage* NotificationSequence[];
extern const NotificationMessage message_do_not_reset;
extern const NotificationSequence sequence_blink_red_100;
extern const NotificationSequence sequence_blink_green_100;
extern const NotificationSequence sequence_display_backlight_enforce_on;
extern const NotificationSequence sequence_display_backlight_enforce_auto;
void notification_message(NotificationApp* app, const NotificationSequence* seq);
