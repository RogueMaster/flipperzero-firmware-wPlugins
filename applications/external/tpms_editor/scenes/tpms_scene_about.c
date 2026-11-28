#include "../tpms_app_i.h"
#include "../helpers/tpms_types.h"

void tpms_scene_about_widget_callback(GuiButtonType result, InputType type, void* context) {
    TPMSApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void tpms_scene_about_on_enter(void* context) {
    TPMSApp* app = context;

    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!          TPMS           \e!\n",
        false);

    FuriString* text = furi_string_alloc();
    furi_string_printf(text, "\e#Information\n");
    furi_string_cat_printf(text, "Version: %s\n", TPMS_VERSION_APP);
    furi_string_cat_printf(text, "Have Fun\n");
    furi_string_cat_printf(text, "Author: altruista86, TxF\n\n");
    furi_string_cat_printf(text, "\e#Features\n");
    furi_string_cat_printf(text, "RX: 19 protocols\n");
    furi_string_cat_printf(text, "TX/editing: 19 formats\n");
    furi_string_cat_printf(text, "Modulations: AM270, AM650,\nFM238 and FM476\n\n");
    furi_string_cat_printf(text, "\e#Protocols\n");
    furi_string_cat_printf(text, "01 Renault\n");
    furi_string_cat_printf(text, "02 Pacific C210/Toyota\n");
    furi_string_cat_printf(text, "03 Schrader\n");
    furi_string_cat_printf(text, "04 Schrader EG53MA4\n");
    furi_string_cat_printf(text, "05 VDO/PSA/FCA\n");
    furi_string_cat_printf(text, "06 Ford\n");
    furi_string_cat_printf(text, "07 BMW Gen2/3\n");
    furi_string_cat_printf(text, "08 Elantra/Honda\n");
    furi_string_cat_printf(text, "09 Hyundai VDO\n");
    furi_string_cat_printf(text, "10 Truck Solar\n");
    furi_string_cat_printf(text, "11 Renault 0435R\n");
    furi_string_cat_printf(text, "12 Honda TRW\n");
    furi_string_cat_printf(text, "13 Porsche\n");
    furi_string_cat_printf(text, "14 Kia\n");
    furi_string_cat_printf(text, "15 Pacific PMV-107J TEST\n");
    furi_string_cat_printf(text, "16 VDO TG1C/Abarth TEST\n");
    furi_string_cat_printf(text, "17 Shenzhen Q85 TEST\n");
    furi_string_cat_printf(text, "18 Mercedes Sprinter TEST\n");
    furi_string_cat_printf(text, "19 Nissan PMV-C811 TEST\n");

    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(text));
    /* Seven short OK presses open the hidden PIN screen. Empty label keeps
       the normal Info page visually unchanged. */
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "", tpms_scene_about_widget_callback, app);
    app->lab_about_taps = 0U;
    app->lab_about_last_tick = 0U;
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewWidget);
}

bool tpms_scene_about_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeCustom && event.event == GuiButtonTypeCenter) {
        const uint32_t now = furi_get_tick();
        if(app->lab_about_last_tick == 0U ||
           (now - app->lab_about_last_tick) > furi_ms_to_ticks(5000U)) {
            app->lab_about_taps = 0U;
        }
        app->lab_about_last_tick = now;
        if(app->lab_about_taps < 7U) app->lab_about_taps++;
        if(app->lab_about_taps >= 7U) {
            app->lab_about_taps = 0U;
            scene_manager_next_scene(app->scene_manager, TPMSSceneLabPin);
        }
        return true;
    }
    return false;
}

void tpms_scene_about_on_exit(void* context) {
    TPMSApp* app = context;
    widget_reset(app->widget);
}
