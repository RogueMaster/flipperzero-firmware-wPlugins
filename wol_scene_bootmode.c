#include "wol_flipper.h"

#define BOOTMODE_EVENT_START 0

static void wol_scene_bootmode_button(GuiButtonType result, InputType type, void* context) {
    WolApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BOOTMODE_EVENT_START);
    }
}

void wol_scene_bootmode_on_enter(void* context) {
    WolApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        48,
        "\e#Put the board in bootloader\n"
        "On the dev board: hold BOOT,\n"
        "tap RESET, release BOOT.\n"
        "The header has no DTR/RTS, so\n"
        "this cannot be done from here.\n"
        "Keep the board on the GPIO\n"
        "pins, 5V is switched on for it.");
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Start", wol_scene_bootmode_button, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewWidget);
}

bool wol_scene_bootmode_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == BOOTMODE_EVENT_START) {
        scene_manager_next_scene(app->scene_manager, WolSceneFlasher);
        return true;
    }
    return false;
}

void wol_scene_bootmode_on_exit(void* context) {
    WolApp* app = context;
    widget_reset(app->widget);
}
