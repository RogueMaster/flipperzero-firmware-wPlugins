#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "../iconedit.h"

#define NOTIFICATION_COUNT 8
static const NotificationMessage* notifications[NOTIFICATION_COUNT];

void notify_error(IconEdit* app) {
    memset(notifications, 0, sizeof(notifications));
    int n = 0;
    notifications[n++] = &message_vibro_on;
    notifications[n++] = &message_red_255;
    notifications[n++] = &message_delay_50;
    notifications[n++] = &message_vibro_off;
    notifications[n++] = &message_red_0;
    notification_message(app->notify, &notifications);
}
