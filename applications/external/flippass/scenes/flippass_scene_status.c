/**
 * @file flippass_scene_status.c
 * @brief Implementation of the shared status/error scene.
 */
#include "flippass_scene_status.h"
#include "../flippass.h"
#include "flippass_scene.h"
#include "flippass_scene_other_field_actions.h"

#include <stdio.h>
#include <string.h>

#define FLIPPASS_STATUS_BOX_CHARS 84U

enum {
    FlipPassSceneStatusEventTypeBluetooth = 0x300U,
    FlipPassSceneStatusEventRefresh,
    FlipPassSceneStatusEventTypeUsb,
    FlipPassSceneStatusEventRunPendingAction,
};

static void flippass_scene_status_button_callback(
    GuiButtonType button_type,
    InputType type,
    void* context) {
    App* app = context;

    if(type != InputTypeShort) {
        return;
    }

    if(button_type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipPassSceneStatusEventTypeBluetooth);
    } else if(button_type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassSceneStatusEventRefresh);
    } else if(button_type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassSceneStatusEventTypeUsb);
    }
}

void flippass_scene_status_show(
    struct App* app,
    const char* title,
    const char* body,
    uint32_t return_scene) {
    flippass_set_status(app, title, body);
    app->status_return_scene = return_scene;
}

static void flippass_scene_status_build_widget(App* app) {
    char compact_message[STATUS_MESSAGE_SIZE + 8];

    snprintf(compact_message, sizeof(compact_message), "\e*%s\e*", app->status_message);
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, app->status_title);
    /* The monospace box shows four 21-character lines; longer messages (for example
     * the pre-password header rejections with their KeePass hint) scroll instead of
     * being cut. A 34-pixel scroll area shows three 11-pixel lines, so the last one
     * never runs under the Back button. Field Show screens keep the monospace box,
     * which keeps revealed values unambiguous. */
    if(!app->other_field_show_active && strlen(app->status_message) > FLIPPASS_STATUS_BOX_CHARS) {
        widget_add_text_scroll_element(app->widget, 0, 16, 128, 34, app->status_message);
    } else {
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 40, AlignLeft, AlignTop, compact_message, true);
    }
    if(app->other_field_show_active) {
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "BT", flippass_scene_status_button_callback, app);
        if(app->other_field_show_totp_refresh) {
            widget_add_button_element(
                app->widget,
                GuiButtonTypeCenter,
                "Refresh",
                flippass_scene_status_button_callback,
                app);
        }
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "USB", flippass_scene_status_button_callback, app);
    } else {
        widget_add_button_element(app->widget, GuiButtonTypeRight, "Back", NULL, NULL);
    }
}

void flippass_scene_status_on_enter(void* context) {
    App* app = context;
    flippass_ui_ensure_widget(app);
    flippass_scene_status_build_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool flippass_scene_status_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeTick && flippass_other_field_show_refresh_due(app)) {
        flippass_other_field_refresh_visible_show(app, app->status_return_scene);
        flippass_scene_status_build_widget(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && app->other_field_show_active) {
        if(event.event == FlipPassSceneStatusEventTypeBluetooth) {
            flippass_other_field_begin_type_action(
                app, true, FlipPassSceneStatusEventRunPendingAction);
            return true;
        }

        if(event.event == FlipPassSceneStatusEventRefresh) {
            flippass_other_field_refresh_visible_show(app, app->status_return_scene);
            flippass_scene_status_build_widget(app);
            return true;
        }

        if(event.event == FlipPassSceneStatusEventTypeUsb) {
            flippass_other_field_begin_type_action(
                app, false, FlipPassSceneStatusEventRunPendingAction);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == FlipPassSceneStatusEventRunPendingAction) {
        flippass_other_field_run_pending_type_action(app, app->status_return_scene);
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, app->status_return_scene)) {
            if(!scene_manager_previous_scene(app->scene_manager)) {
                flippass_request_exit(app);
            }
        }
        return true;
    }

    return false;
}

void flippass_scene_status_on_exit(void* context) {
    App* app = context;
    flippass_other_field_clear_show_state(app);
    if(app->widget != NULL) {
        widget_reset(app->widget);
    }
}
