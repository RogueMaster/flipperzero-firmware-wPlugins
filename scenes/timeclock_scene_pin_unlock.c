// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Startup unlock screen shown when a PIN is configured. Back is blocked here:
// the app cannot be left without entering the PIN.

static char unlock_msg[24];

static void timeclock_scene_pin_unlock_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TimeClockCustomEventPinEntered);
}

static void timeclock_scene_pin_unlock_setup(TimeClock* app, const char* message) {
    pin_view_reset(app->pin_view, "Enter PIN");
    pin_view_set_callback(app->pin_view, timeclock_scene_pin_unlock_callback, app);
    if(message) {
        pin_view_set_message(app->pin_view, message);
    } else if(app->config.attempts > 0) {
        snprintf(
            unlock_msg,
            sizeof(unlock_msg),
            "Attempts %lu/%u",
            (unsigned long)app->config.attempts,
            TC_MAX_PIN_ATTEMPTS);
        pin_view_set_message(app->pin_view, unlock_msg);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPin);
}

void timeclock_scene_pin_unlock_on_enter(void* context) {
    TimeClock* app = context;
    timeclock_scene_pin_unlock_setup(app, NULL);
}

bool timeclock_scene_pin_unlock_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == TimeClockCustomEventPinEntered) {
        consumed = true;

        char code[TC_PIN_LEN + 1];
        pin_view_get_code(app->pin_view, code, sizeof(code));

        if(tc_pin_hash(code, app->config.pin_salt) == app->config.pin_hash) {
            app->config.attempts = 0;
            tc_config_save(&app->config);
            timeclock_notify_success(app);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneMenu);
        } else {
            app->config.attempts++;
            tc_config_save(&app->config);
            timeclock_notify_error(app);

            snprintf(
                unlock_msg,
                sizeof(unlock_msg),
                "Wrong PIN %lu/%u",
                (unsigned long)app->config.attempts,
                TC_MAX_PIN_ATTEMPTS);

            // Simple lockout penalty after too many wrong attempts.
            if(app->config.attempts >= TC_MAX_PIN_ATTEMPTS) {
                furi_delay_ms(3000);
            }
            timeclock_scene_pin_unlock_setup(app, unlock_msg);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Block leaving the app from the unlock screen.
        consumed = true;
    }

    return consumed;
}

void timeclock_scene_pin_unlock_on_exit(void* context) {
    UNUSED(context);
}
