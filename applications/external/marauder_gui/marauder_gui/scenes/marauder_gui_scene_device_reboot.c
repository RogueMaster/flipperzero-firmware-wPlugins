#include "../marauder_gui_app_i.h"

/* "reboot" (REBOOT_CMD) just calls ESP.restart() on the ESP32 side - no confirmation message,
   no @MARAUDER status line, nothing to wait on. The serial link (and every scan/attack) drops
   for a few seconds while it comes back up; this screen only tells the user that's expected. */

enum {
    DeviceRebootEventOk,
};

static void marauder_gui_scene_device_reboot_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DeviceRebootEventOk);
    }
}

void marauder_gui_scene_device_reboot_on_enter(void* context) {
    MarauderGuiApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Reboot");
    widget_add_text_box_element(
        app->widget,
        0,
        16,
        128,
        30,
        AlignCenter,
        AlignTop,
        marauder_gui_text(
            app,
            "ESP32 yeniden baslatiliyor.\nSeri baglanti birkac\nsaniyeligine kesilecek.",
            "ESP32 is restarting.\nThe serial connection will\ndrop for a few seconds."),
        false);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Tamam", "OK"),
        marauder_gui_scene_device_reboot_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_uart_send_line(app->uart, "reboot");
}

bool marauder_gui_scene_device_reboot_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == DeviceRebootEventOk) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_device_reboot_on_exit(void* context) {
    MarauderGuiApp* app = context;
    widget_reset(app->widget);
}
