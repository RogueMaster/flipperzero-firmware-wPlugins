#include "../tpms_app_i.h"

static void
    tpms_scene_sweep_result_widget_callback(GuiButtonType result, InputType type, void* context) {
    TPMSApp* app = context;
    UNUSED(result);
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

void tpms_scene_sweep_result_on_enter(void* context) {
    TPMSApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    if(app->sweep_found_signal) {
        // Success - signal was found
        widget_add_string_element(
            widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Sweep Complete!");

        widget_add_string_element(
            widget, 64, 20, AlignCenter, AlignTop, FontSecondary, "Signal Found!");

        // Show frequency
        char freq_str[32];
        uint32_t freq_mhz = app->sweep_result_frequency / 1000000;
        uint32_t freq_khz = (app->sweep_result_frequency % 1000000) / 1000;
        snprintf(freq_str, sizeof(freq_str), "Freq: %lu.%02lu MHz", freq_mhz, freq_khz / 10);
        widget_add_string_element(widget, 64, 32, AlignCenter, AlignTop, FontSecondary, freq_str);

        // Show modulation
        char mod_str[32];
        snprintf(mod_str, sizeof(mod_str), "Mod: %s", app->sweep_result_preset);
        widget_add_string_element(widget, 64, 44, AlignCenter, AlignTop, FontSecondary, mod_str);

    } else {
        // Failure - no signal found
        widget_add_string_element(
            widget, 64, 10, AlignCenter, AlignTop, FontPrimary, "Sweep Complete");

        widget_add_string_element(
            widget, 64, 28, AlignCenter, AlignTop, FontSecondary, "No signal detected");

        widget_add_string_element(
            widget, 64, 40, AlignCenter, AlignTop, FontSecondary, "after all cycles.");
    }

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "OK", tpms_scene_sweep_result_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewWidget);
}

bool tpms_scene_sweep_result_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack || event.type == SceneManagerEventTypeCustom) {
        // Go back to start
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TPMSSceneStart);
        consumed = true;
    }

    return consumed;
}

void tpms_scene_sweep_result_on_exit(void* context) {
    TPMSApp* app = context;
    widget_reset(app->widget);
}
