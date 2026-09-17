// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Multi-step PIN scene driven by app->pin_mode:
//   SetNew     -> enter new PIN, then ConfirmNew
//   ConfirmNew -> re-enter; on match, enable the PIN and return
//   VerifyOld  -> verify current PIN; scene state 0 = change (-> SetNew),
//                 1 = disable the PIN
//   VerifyExit -> verify current PIN; on success, leave the app
//
// Set/Change/Disable all end on a brief confirmation popup (matching the
// Export scene's pattern) instead of silently returning to Settings, so the
// result is never in doubt even with sound/vibro/LED all off.

#define PIN_POPUP_DONE 280u

static char pin_msg[24];

static void timeclock_scene_pin_set_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TimeClockCustomEventPinEntered);
}

static void timeclock_scene_pin_set_popup_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PIN_POPUP_DONE);
}

// Show a brief confirmation, then return to Settings.
static void timeclock_scene_pin_set_show_done(TimeClock* app, const char* header, const char* text) {
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(popup, text, 64, 30, AlignCenter, AlignTop);
    popup_set_callback(popup, timeclock_scene_pin_set_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 1200);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
}

static void timeclock_scene_pin_set_setup(TimeClock* app, const char* message) {
    const char* title = tc_str(StrSetPin);
    switch(app->pin_mode) {
    case TcPinModeSetNew:
        title = tc_str(StrSetPin);
        break;
    case TcPinModeConfirmNew:
        title = tc_str(StrConfirmPin);
        break;
    case TcPinModeVerifyOld:
        title = tc_str(StrCurrentPin);
        break;
    case TcPinModeVerifyExit:
    case TcPinModeVerifyExitWork:
        title = tc_str(StrPinToExit);
        break;
    default:
        break;
    }

    pin_view_reset(app->pin_view, title);
    pin_view_set_callback(app->pin_view, timeclock_scene_pin_set_callback, app);
    pin_view_set_message(app->pin_view, message ? message : tc_str(StrPinHint));
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

    if(event.type == SceneManagerEventTypeCustom && event.event == PIN_POPUP_DONE) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

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
                timeclock_scene_pin_set_show_done(app, tc_str(StrDone), tc_str(StrPinSaved));
            } else {
                timeclock_notify_error(app);
                memset(app->pin_new, 0, sizeof(app->pin_new));
                app->pin_mode = TcPinModeSetNew;
                timeclock_scene_pin_set_setup(app, tc_str(StrMismatch));
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
                    timeclock_scene_pin_set_show_done(app, tc_str(StrDone), tc_str(StrPinDisabled));
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
                    "%s %lu/%u",
                    tc_str(StrWrongPin),
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
                    "%s %lu/%u",
                    tc_str(StrWrongPin),
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
                    "%s %lu/%u",
                    tc_str(StrWrongPin),
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
    TimeClock* app = context;
    popup_reset(app->popup);
}
