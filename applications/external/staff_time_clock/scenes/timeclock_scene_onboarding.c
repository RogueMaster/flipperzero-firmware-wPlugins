// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// First-launch onboarding: explains the optional protection PIN and lets the
// user set it right away (arrow sequence) or skip. Shown once (config.onboarded).

#define ONB_SKIP 270u
#define ONB_SETPIN 271u
#define ONB_DONE 272u

static void timeclock_scene_onboarding_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TimeClock* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ONB_SKIP);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ONB_SETPIN);
    }
}

void timeclock_scene_onboarding_on_enter(void* context) {
    TimeClock* app = context;

    // If the PIN was just set during onboarding, we're done: pop to the menu.
    // Defer via a custom event (do not manipulate the stack inside on_enter).
    if(app->config.pin_enabled) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ONB_DONE);
        return;
    }

    Widget* widget = app->widget;
    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Staff Time Clock");
    widget_add_string_multiline_element(
        widget, 64, 20, AlignCenter, AlignTop, FontSecondary, tc_str(StrOnbText));
    widget_add_button_element(
        widget, GuiButtonTypeLeft, tc_str(StrSkip), timeclock_scene_onboarding_button_callback, app);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        tc_str(StrSetPin),
        timeclock_scene_onboarding_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWidget);
}

bool timeclock_scene_onboarding_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ONB_SKIP) {
            app->config.onboarded = true;
            tc_config_save(&app->config);
            scene_manager_previous_scene(app->scene_manager); // -> menu
            consumed = true;
        } else if(event.event == ONB_SETPIN) {
            app->config.onboarded = true;
            tc_config_save(&app->config);
            app->pin_mode = TcPinModeSetNew;
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            consumed = true;
        } else if(event.event == ONB_DONE) {
            scene_manager_previous_scene(app->scene_manager); // -> menu
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back = skip: remember it and let the scene pop to the menu.
        app->config.onboarded = true;
        tc_config_save(&app->config);
        consumed = false;
    }

    return consumed;
}

void timeclock_scene_onboarding_on_exit(void* context) {
    TimeClock* app = context;
    widget_reset(app->widget);
}
