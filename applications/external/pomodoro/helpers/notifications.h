#pragma once

#include "../modules/flipp_pomodoro.h"
#include <notification/notification_messages.h>

/// @brief Focus-stage start cue (green + tones + vibro)
extern const NotificationSequence work_start_notification;
/// @brief Short-rest start cue (red + tones + vibro)
extern const NotificationSequence rest_start_notification;
/// @brief Long-break start cue (blue + “dolphin” motif + vibro)
extern const NotificationSequence long_break_start_notification;

/// @brief Defines a notification sequence that should indicate start of specific pomodoro stage.
extern const NotificationSequence* stage_start_notification_sequence_map[];

/// @brief Stops any sound/vibro immediately.
extern const NotificationSequence stop_all_notification;

/// @brief Flash backlight on (green pulse)
extern const NotificationSequence flash_backlight_on_sequence;
/// @brief Flash backlight off (green off)
extern const NotificationSequence flash_backlight_off_sequence;
/// @brief Single short vibrate
extern const NotificationSequence vibrate_sequence;
/// @brief Soft single beep (D5)
extern const NotificationSequence soft_beep_sequence;
/// @brief Louder triple beep (D5/B5/D5)
extern const NotificationSequence loud_beep_sequence;
