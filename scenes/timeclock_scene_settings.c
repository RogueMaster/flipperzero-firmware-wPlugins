// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Settings: sound/vibro/LED, language, PIN management and exit. The reader is
// fully automatic (NFC + RFID + iButton at once), so there is nothing to pick.

typedef enum {
    ActionSound = 0,
    ActionVibro,
    ActionLed,
    ActionLanguage,
    ActionSetPin,
    ActionChangePin,
    ActionDisablePin,
    ActionExit,
} SettingsAction;

static char sound_lbl[28];
static char vibro_lbl[28];
static char led_lbl[28];
static char lang_lbl[28];

static void timeclock_scene_settings_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void timeclock_scene_settings_build(TimeClock* app, uint8_t sel_pos) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, tc_str(StrSettings));

    const char* on = tc_str(StrOn);
    const char* off = tc_str(StrOff);
    snprintf(
        sound_lbl, sizeof(sound_lbl), "%s: %s", tc_str(StrSound), app->config.sound_enabled ? on : off);
    snprintf(
        vibro_lbl, sizeof(vibro_lbl), "%s: %s", tc_str(StrVibro), app->config.vibro_enabled ? on : off);
    snprintf(led_lbl, sizeof(led_lbl), "%s: %s", tc_str(StrLed), app->config.led_enabled ? on : off);
    snprintf(
        lang_lbl,
        sizeof(lang_lbl),
        "%s: %s",
        tc_str(StrLanguage),
        tc_lang_name((TcLang)app->config.language));

    submenu_add_item(
        submenu, sound_lbl, ActionSound, timeclock_scene_settings_submenu_callback, app);
    submenu_add_item(
        submenu, vibro_lbl, ActionVibro, timeclock_scene_settings_submenu_callback, app);
    submenu_add_item(submenu, led_lbl, ActionLed, timeclock_scene_settings_submenu_callback, app);
    submenu_add_item(
        submenu, lang_lbl, ActionLanguage, timeclock_scene_settings_submenu_callback, app);

    if(!app->config.pin_enabled) {
        submenu_add_item(
            submenu, tc_str(StrSetPin), ActionSetPin, timeclock_scene_settings_submenu_callback, app);
    } else {
        submenu_add_item(
            submenu,
            tc_str(StrChangePin),
            ActionChangePin,
            timeclock_scene_settings_submenu_callback,
            app);
        submenu_add_item(
            submenu,
            tc_str(StrDisablePin),
            ActionDisablePin,
            timeclock_scene_settings_submenu_callback,
            app);
    }

    submenu_add_item(
        submenu, tc_str(StrExitApp), ActionExit, timeclock_scene_settings_submenu_callback, app);

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
        case ActionSound:
            app->config.sound_enabled = !app->config.sound_enabled;
            tc_config_save(&app->config);
            timeclock_scene_settings_build(app, 0);
            break;
        case ActionVibro:
            app->config.vibro_enabled = !app->config.vibro_enabled;
            tc_config_save(&app->config);
            timeclock_scene_settings_build(app, 1);
            break;
        case ActionLed:
            app->config.led_enabled = !app->config.led_enabled;
            tc_config_save(&app->config);
            timeclock_scene_settings_build(app, 2);
            break;
        case ActionLanguage:
            app->config.language = (app->config.language + 1) % TcLangCount;
            tc_lang_set((TcLang)app->config.language);
            tc_config_save(&app->config);
            timeclock_scene_settings_build(app, 3);
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
