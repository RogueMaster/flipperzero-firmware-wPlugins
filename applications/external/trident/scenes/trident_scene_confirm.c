#include "../trident_i.h"
#include <stdio.h>

static void trident_scene_confirm_button_cb(GuiButtonType result, InputType type, void* context) {
    TridentApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TridentCustomEventConfirmStart);
    }
}

void trident_scene_confirm_on_enter(void* context) {
    TridentApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_icon_element(widget, 2, 2, &I_skull_10px);

    char head[40];
    snprintf(head, sizeof(head), "Start: %s", app->pending_title);
    widget_add_string_element(widget, 16, 3, AlignLeft, AlignTop, FontPrimary, head);

    widget_add_text_box_element(
        widget,
        0,
        16,
        128,
        34,
        AlignLeft,
        AlignTop,
        "This transmits and can disrupt\n"
        "nearby devices. Only run it on\n"
        "networks you own or are\n"
        "authorised to test.",
        false);

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Start", trident_scene_confirm_button_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewWidget);
}

bool trident_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == TridentCustomEventConfirmStart) {
        trident_notify_start(app);
        // Replace the confirm scene with the console so Back skips the prompt.
        scene_manager_previous_scene(app->scene_manager);
        scene_manager_next_scene(app->scene_manager, TridentSceneConsole);
        consumed = true;
    }
    return consumed;
}

void trident_scene_confirm_on_exit(void* context) {
    TridentApp* app = context;
    widget_reset(app->widget);
}
