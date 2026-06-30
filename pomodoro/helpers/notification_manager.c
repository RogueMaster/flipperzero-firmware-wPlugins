#include "notification_manager.h"
#include "notifications.h"
#include "../helpers/time.h"
#include <notification/notification.h>
#include <furi.h>

// Notification behavior constants
#define ANNOYING_MODE_REPEAT_COUNT 10

// Handler return value signals
#define SIGNAL_SEND_STAGE_COMPLETE_EVENT true
#define SIGNAL_NO_STAGE_COMPLETE_EVENT false

struct NotificationManager {
    bool stage_complete_sent;
    bool notification_started;
    uint8_t notification_repeats_left;
    uint32_t notification_cooldown_until;
    bool flash_backlight_on;
};

NotificationManager* notification_manager_alloc(void) {
    NotificationManager* manager = malloc(sizeof(NotificationManager));
    furi_assert(manager);
    notification_manager_reset(manager);
    return manager;
}

void notification_manager_free(NotificationManager* manager) {
    furi_assert(manager);
    free(manager);
}

void notification_manager_reset(NotificationManager* manager) {
    furi_assert(manager);
    manager->stage_complete_sent = false;
    manager->notification_started = false;
    manager->notification_repeats_left = 0;
    manager->notification_cooldown_until = 0;
    manager->flash_backlight_on = false;
}

void notification_manager_reset_flags(NotificationManager* manager) {
    furi_assert(manager);
    notification_manager_reset(manager);
}

static void send_notification_sequence(const NotificationSequence* sequence) {
    if(!sequence) {
        return;
    }
    NotificationApp* notification_app = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification_app, sequence);
    furi_record_close(RECORD_NOTIFICATION);
}

static void stop_all_notifications(void) {
    NotificationApp* notification_app = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification_app, &stop_all_notification);
    furi_record_close(RECORD_NOTIFICATION);
}

void notification_manager_stop(NotificationManager* manager) {
    furi_assert(manager);
    manager->notification_repeats_left = 0;
    manager->notification_cooldown_until = time_now() + 1;
    manager->notification_started = false;
    manager->flash_backlight_on = false;
    stop_all_notifications();
}

static void toggle_flash_pattern(NotificationManager* manager) {
    send_notification_sequence(manager->flash_backlight_on ? &flash_backlight_off_sequence : &flash_backlight_on_sequence);
    manager->flash_backlight_on = !manager->flash_backlight_on;
}

static bool is_cooldown_active(NotificationManager* manager) {
    const uint32_t current_time = time_now();
    return current_time < manager->notification_cooldown_until;
}

static void set_cooldown(NotificationManager* manager, uint32_t cooldown_seconds) {
    const uint32_t current_time = time_now();
    manager->notification_started = true;
    manager->notification_cooldown_until = current_time + cooldown_seconds;
}

static void notify_next_stage(NotificationManager* manager, PomodoroStage next_stage) {
    if(is_cooldown_active(manager)) {
        return;
    }

    const NotificationSequence* notification_sequence = stage_start_notification_sequence_map[next_stage];

    send_notification_sequence(notification_sequence);
    manager->notification_cooldown_until = time_now() + 3;
}

static bool handle_buzz_slide(NotificationManager* manager) {
    if(!manager->stage_complete_sent) {
        manager->stage_complete_sent = true;
        return SIGNAL_SEND_STAGE_COMPLETE_EVENT;
    }
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

static bool handle_buzz_once(NotificationManager* manager, PomodoroStage next_stage) {
    if(!manager->notification_started) {
        manager->notification_started = true;
        notify_next_stage(manager, next_stage);
    }
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

static bool handle_buzz_annoying(NotificationManager* manager, PomodoroStage next_stage) {
    if(!manager->notification_started) {
        manager->notification_started = true;
        manager->notification_repeats_left = ANNOYING_MODE_REPEAT_COUNT;
    }
    
    if(manager->notification_repeats_left > 0) {
        notify_next_stage(manager, next_stage);
        manager->notification_repeats_left--;
    }
    
    if(manager->notification_repeats_left <= 0) {
        stop_all_notifications();
    }
    
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

static bool handle_buzz_flash(NotificationManager* manager) {
    if(is_cooldown_active(manager)) {
        return SIGNAL_NO_STAGE_COMPLETE_EVENT;
    }
    toggle_flash_pattern(manager);
    set_cooldown(manager, 1);
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

static bool handle_buzz_vibrate(NotificationManager* manager) {
    if(is_cooldown_active(manager)) {
        return SIGNAL_NO_STAGE_COMPLETE_EVENT;
    }
    send_notification_sequence(&vibrate_sequence);
    toggle_flash_pattern(manager);
    set_cooldown(manager, 1);
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

static bool handle_buzz_soft_beep(NotificationManager* manager) {
    if(is_cooldown_active(manager)) {
        return SIGNAL_NO_STAGE_COMPLETE_EVENT;
    }
    send_notification_sequence(&soft_beep_sequence);
    set_cooldown(manager, 2);
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

static bool handle_buzz_loud_beep(NotificationManager* manager) {
    if(is_cooldown_active(manager)) {
        return SIGNAL_NO_STAGE_COMPLETE_EVENT;
    }
    send_notification_sequence(&loud_beep_sequence);
    set_cooldown(manager, 3);
    return SIGNAL_NO_STAGE_COMPLETE_EVENT;
}

bool notification_manager_handle_expired_stage(
    NotificationManager* manager,
    PomodoroStage next_stage,
    FlippPomodoroBuzzMode buzz_mode) {
    
    furi_assert(manager);
    
    switch(buzz_mode) {
        case FlippPomodoroBuzzSlide:
            return handle_buzz_slide(manager);
        case FlippPomodoroBuzzOnce:
            return handle_buzz_once(manager, next_stage);
        case FlippPomodoroBuzzAnnoying:
            return handle_buzz_annoying(manager, next_stage);
        case FlippPomodoroBuzzFlash:
            return handle_buzz_flash(manager);
        case FlippPomodoroBuzzVibrate:
            return handle_buzz_vibrate(manager);
        case FlippPomodoroBuzzSoftBeep:
            return handle_buzz_soft_beep(manager);
        case FlippPomodoroBuzzLoudBeep:
            return handle_buzz_loud_beep(manager);
        default:
            return handle_buzz_once(manager, next_stage);
    }
}
