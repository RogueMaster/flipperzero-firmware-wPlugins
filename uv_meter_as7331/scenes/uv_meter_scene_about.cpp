#include "uv_meter_app_i.hpp"

#define UV_METER_VERSION_APP FAP_VERSION
#define UV_METER_DEVELOPER "Michael Baisch"
#define UV_METER_GITHUB "https://github.com/michaelbaisch/uv_meter"
#define UV_METER_NAME "\e#\e!             UV Meter             \e!\n"
#define UV_METER_BLANK_INV "\e#\e!                                                      \e!\n"

void uv_meter_scene_about_on_enter(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    widget_reset(app->widget);
    FuriString* tmp_string = furi_string_alloc();

    widget_add_text_box_element(
        app->widget, 0, 0, 128, 14, AlignCenter, AlignBottom, UV_METER_BLANK_INV, false);
    widget_add_text_box_element(
        app->widget, 0, 2, 128, 14, AlignCenter, AlignBottom, UV_METER_NAME, false);
    furi_string_printf(tmp_string, "\e#Information\n");
    furi_string_cat_printf(tmp_string, "Version: %s\n", UV_METER_VERSION_APP);
    furi_string_cat_printf(tmp_string, "Developed by: %s\n", UV_METER_DEVELOPER);
    furi_string_cat_printf(tmp_string, "Github: %s\n\n", UV_METER_GITHUB);
    furi_string_cat_str(tmp_string, "\e#Description\n");
    furi_string_cat_str(tmp_string, "Measure UV radiation using\n");
    furi_string_cat_str(tmp_string, "the AS7331 sensor.\n");
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(tmp_string));

    furi_string_free(tmp_string);
    view_dispatcher_switch_to_view(app->view_dispatcher, UVMeterViewWidget);
}

bool uv_meter_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void uv_meter_scene_about_on_exit(void* context) {
    furi_assert(context);
    auto* app = static_cast<UVMeterApp*>(context);
    widget_reset(app->widget);
}