#include "../chameleon_app_i.h"

void chameleon_scene_diagnostic_on_enter(void* context) {
    ChameleonApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    // Get device info
    chameleon_app_get_device_info(app);

    char info_text[256];
    snprintf(
        info_text,
        sizeof(info_text),
        "Chameleon Ultra\nDiagnostic Info\n\n"
        "Firmware: %d.%d\n"
        "Model: %s\n"
        "Mode: %s\n"
        "Chip ID: %llX\n"
        "Connection: %s",
        app->device_info.major_version,
        app->device_info.minor_version,
        app->device_info.model == ChameleonModelUltra ? "Ultra" : "Lite",
        app->device_info.mode == ChameleonModeReader ? "Reader" : "Emulator",
        app->device_info.chip_id,
        app->connection_type == ChameleonConnectionUSB ? "USB" : "Bluetooth");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, info_text);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewWidget);
}

bool chameleon_scene_diagnostic_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void chameleon_scene_diagnostic_on_exit(void* context) {
    ChameleonApp* app = context;
    widget_reset(app->widget);
}
