// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Multi-step PIN scene driven by app->pin_mode:
//   SetNew     -> enter new PIN, then ConfirmNew
//   ConfirmNew -> re-enter; on match, enable the PIN and return
//   VerifyOld  -> verify current PIN; scene state 0 = change (-> SetNew),
//                 1 = disable the PIN
//   VerifyExit -> verify current PIN; on success, leave the app

static char pin_msg[24];

static void timeclock_scene_pin_set_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TimeClockCustomEventPinEntered);
}

static void timeclock_scene_pin_set_setup(TimeClock* app, const char* message) {
    const char* title = "PIN";
    switch(app->pin_mode) {
    case TcPinModeSetNew:
        title = "Set PIN";
        break;
    case TcPinModeConfirmNew:
        title = "Confirm PIN";
        break;
    case TcPinModeVerifyOld:
        title = "Current PIN";
        break;
    case TcPinModeVerifyExit:
        title = "PIN to exit";
        break;
    case TcPinModeVerifyExitWork:
        title = "PIN to exit";
        break;
    default:
        break;
    }

    pin_view_reset(app->pin_view, title);
    pin_view_set_callback(app->pin_view, timeclock_scene_pin_set_callback, app);
    if(message) pin_view_set_message(app->pin_view, message);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPin);
}

static bool timeclock_scene_pin_verify(TimeClock* app, const char* code) {
    return tc_pin_hash(code, app->config.pin_salt) == app->config.pin_hash;
}

static void timeclock_scene_pin_finalize_enable(TimeClock* app) {
    uint32_t salt = tc_pin_make_salt();
    app->config.pin_salt = salt;
    app->config.pin_hash = tc_pin_hash(app->pin_new, salt);
    app->config.pin_enabled = true;
    app->config.attempts = 0;
    tc_config_save(&app->config);
    // The plaintext PIN is no longer needed: wipe it from RAM.
    memset(app->pin_new, 0, sizeof(app->pin_new));
}

void timeclock_scene_pin_set_on_enter(void* context) {
    TimeClock* app = context;
    timeclock_scene_pin_set_setup(app, NULL);
}

bool timeclock_scene_pin_set_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == TimeClockCustomEventPinEntered) {
        consumed = true;

        char code[TC_PIN_LEN + 1];
        pin_view_get_code(app->pin_view, code, sizeof(code));

        switch(app->pin_mode) {
        case TcPinModeSetNew:
            strncpy(app->pin_new, code, sizeof(app->pin_new) - 1);
            app->pin_new[sizeof(app->pin_new) - 1] = '\0';
            app->pin_mode = TcPinModeConfirmNew;
            timeclock_scene_pin_set_setup(app, NULL);
            break;

        case TcPinModeConfirmNew:
            if(strcmp(code, app->pin_new) == 0) {
                timeclock_scene_pin_finalize_enable(app);
                timeclock_notify_success(app);
                scene_manager_previous_scene(app->scene_manager);
            } else {
                timeclock_notify_error(app);
                memset(app->pin_new, 0, sizeof(app->pin_new));
                app->pin_mode = TcPinModeSetNew;
                timeclock_scene_pin_set_setup(app, "Mismatch, retry");
            }
            break;

        case TcPinModeVerifyOld:
            if(timeclock_scene_pin_verify(app, code)) {
                uint32_t intent = scene_manager_get_scene_state(
                    app->scene_manager, TimeClockScenePinSet);
                if(intent == 1) {
                    // Disable the PIN.
                    app->config.pin_enabled = false;
                    app->config.pin_hash = 0;
                    app->config.pin_salt = 0;
                    app->config.attempts = 0;
                    tc_config_save(&app->config);
                    timeclock_notify_success(app);
                    scene_manager_previous_scene(app->scene_manager);
                } else {
                    // Change: proceed to set a new PIN.
                    app->pin_mode = TcPinModeSetNew;
                    timeclock_scene_pin_set_setup(app, NULL);
                }
            } else {
                app->config.attempts++;
                tc_config_save(&app->config);
                timeclock_notify_error(app);
                snprintf(
                    pin_msg,
                    sizeof(pin_msg),
                    "Wrong PIN %lu/%u",
                    (unsigned long)app->config.attempts,
                    TC_MAX_PIN_ATTEMPTS);
                timeclock_scene_pin_set_setup(app, pin_msg);
            }
            break;

        case TcPinModeVerifyExit:
            if(timeclock_scene_pin_verify(app, code)) {
                app->config.attempts = 0;
                tc_config_save(&app->config);
                view_dispatcher_stop(app->view_dispatcher);
            } else {
                app->config.attempts++;
                tc_config_save(&app->config);
                timeclock_notify_error(app);
                snprintf(
                    pin_msg,
                    sizeof(pin_msg),
                    "Wrong PIN %lu/%u",
                    (unsigned long)app->config.attempts,
                    TC_MAX_PIN_ATTEMPTS);
                timeclock_scene_pin_set_setup(app, pin_msg);
            }
            break;

        case TcPinModeVerifyExitWork:
            if(timeclock_scene_pin_verify(app, code)) {
                app->config.attempts = 0;
                tc_config_save(&app->config);
                // Leave Work mode back to the main menu.
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, TimeClockSceneMenu);
            } else {
                app->config.attempts++;
                tc_config_save(&app->config);
                timeclock_notify_error(app);
                snprintf(
                    pin_msg,
                    sizeof(pin_msg),
                    "Wrong PIN %lu/%u",
                    (unsigned long)app->config.attempts,
                    TC_MAX_PIN_ATTEMPTS);
                timeclock_scene_pin_set_setup(app, pin_msg);
            }
            break;

        default:
            break;
        }

        // Do not keep the entered digits in RAM longer than needed.
        memset(code, 0, sizeof(code));
    }

    return consumed;
}

void timeclock_scene_pin_set_on_exit(void* context) {
    UNUSED(context);
}
