#pragma once

#include "lib/hardware/notification/Notification.hpp"

const NotificationSequence NOTIFICATION_PAGER_RECEIVE = {
    &message_vibro_on,
    &message_note_e6,

    &message_blue_255,
    &message_delay_50,

    &message_sound_off,
    &message_vibro_off,

    NULL,
};
