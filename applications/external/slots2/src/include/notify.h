/*
 * File: notify.h
 * Author: vh8t
 * Created: 28/12/2025
 */

#ifndef NOTIFY_H
#define NOTIFY_H

typedef enum {
    NOTIFICATION_WIN,
    NOTIFICATION_NO_BALANCE,
    NOTIFICATION_CHEAT_ENABLED,
} notification_t;

void notify(notification_t notification);

#endif // NOTIFY_H
