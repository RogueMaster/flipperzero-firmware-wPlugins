// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Settings: toggle auto mode and reader type, manage the PIN, and exit the app
// (protected by the PIN when one is set).

typedef enum {
    ActionAuto = 0,
    ActionReader = 1,
    ActionSetPin = 2,
    ActionChangePin = 3,
    ActionDisablePin = 4,
    ActionExit = 5,
} SettingsAction;

static char auto_lbl[24];
static char reader_lbl[24];

static void timeclock_scene_settings_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void timeclock_scene_settings_build(TimeClock* app, uint8_t sel_pos) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Settings");

    snprintf(auto_lbl, sizeof(auto_lbl), "Auto mode: %s", app->config.auto_mode ? "On" : "Off");
    snprintf(reader_lbl, sizeof(reader_lbl), "Reader: %s", app->config.use_lf ? "RFID" : "NFC");

    submenu_add_item(
        submenu, auto_lbl, ActionAuto, timeclock_scene_settings_submenu_callback, app);
    submenu_add_item(
        submenu, reader_lbl, ActionReader, timeclock_scene_settings_submenu_callback, app);

    if(!app->config.pin_enabled) {
        submenu_add_item(
            submenu, "Set PIN", ActionSetPin, timeclock_scene_settings_submenu_callback, app);
    } else {
        submenu_add_item(
            submenu,
            "Change PIN",
            ActionChangePin,
            timeclock_scene_settings_submenu_callback,
            app);
        submenu_add_item(
            submenu,
            "Disable PIN",
            ActionDisablePin,
            timeclock_scene_settings_submenu_callback,
            app);
    }

    submenu_add_item(
        submenu, "Exit app", ActionExit, timeclock_scene_settings_submenu_callback, app);

    submenu_set_selected_item(submenu, sel_pos);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

void timeclock_scene_settings_on_enter(void* context) {
    TimeClock* app = context;
    timeclock_scene_settings_build(app, 0);
}

bool timeclock_scene_settings_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ActionAuto:
            app->config.auto_mode = !app->config.auto_mode;
            tc_config_save(&app->config);
            timeclock_scene_settings_build(app, 0);
            break;
        case ActionReader:
            app->config.use_lf = !app->config.use_lf;
            tc_config_save(&app->config);
            timeclock_scene_settings_build(app, 1);
            break;
        case ActionSetPin:
            app->pin_mode = TcPinModeSetNew;
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            break;
        case ActionChangePin:
            app->pin_mode = TcPinModeVerifyOld;
            scene_manager_set_scene_state(app->scene_manager, TimeClockScenePinSet, 0); // change
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            break;
        case ActionDisablePin:
            app->pin_mode = TcPinModeVerifyOld;
            scene_manager_set_scene_state(app->scene_manager, TimeClockScenePinSet, 1); // disable
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            break;
        case ActionExit:
            if(app->config.pin_enabled) {
                app->pin_mode = TcPinModeVerifyExit;
                scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            } else {
                view_dispatcher_stop(app->view_dispatcher);
            }
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void timeclock_scene_settings_on_exit(void* context) {
    UNUSED(context);
}
