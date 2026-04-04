/**
 * @file flippass_scene_status.c
 * @brief Implementation of the shared status/error scene.
 */
#include "flippass_scene_status.h"
#include "../flippass.h"
#include "flippass_scene.h"

#include <stdio.h>

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
    widget_add_text_box_element(
        app->widget,
        0,
        16,
        128,
        40,
        AlignLeft,
        AlignTop,
        compact_message,
        true);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Back", NULL, NULL);
}

void flippass_scene_status_on_enter(void* context) {
    App* app = context;
    flippass_scene_status_build_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool flippass_scene_status_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

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
    widget_reset(app->widget);
}
