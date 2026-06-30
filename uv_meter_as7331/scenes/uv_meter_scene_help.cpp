#include "uv_meter_app_i.hpp"

void uv_meter_scene_help_on_enter(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    widget_reset(app->widget);
    FuriString* tmp_string = furi_string_alloc();

    furi_string_cat_str(tmp_string, "\e#Wiring:\n");
    furi_string_cat_str(
        tmp_string,
        "  SCL: 16 [C0]\n"
        "  SDA: 15 [C1]\n"
        "  3V3: 9 [3V3]\n"
        "  GND: 11 or 18 [GND]\n");
    furi_string_cat_str(tmp_string, "\e#Usage:\n");
    furi_string_cat_str(
        tmp_string,
        "Main UV values shown with\n"
        "raw value meters beside.\n"
        "Avoid low/high warnings by\n"
        "adjusting Gain/Exposure.\n"
        "Right side: Max daily safe\n"
        "UV exposure (minutes per\n"
        "8h day).\n"
        "Percentages indicates each\n"
        "UV type's contribution.\n");
    furi_string_cat_str(
        tmp_string,
        "\e#Disclaimer\n"
        "Info provided as-is;\n"
        "not liable for usage.\n");

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(tmp_string));

    furi_string_free(tmp_string);
    view_dispatcher_switch_to_view(app->view_dispatcher, UVMeterViewWidget);
}

bool uv_meter_scene_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void uv_meter_scene_help_on_exit(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    widget_reset(app->widget);
}