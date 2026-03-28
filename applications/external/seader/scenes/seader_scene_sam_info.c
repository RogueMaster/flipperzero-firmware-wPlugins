#include "../seader_i.h"
#include <dolphin/dolphin.h>

#define TAG "SeaderSamInfoScene"

void seader_scene_sam_info_widget_callback(GuiButtonType result, InputType type, void* context) {
    Seader* seader = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seader->view_dispatcher, result);
    }
}

void seader_scene_sam_info_on_enter(void* context) {
    Seader* seader = context;
    Widget* widget = seader_get_widget(seader);
    if(!widget) {
        FURI_LOG_E(TAG, "Widget view unavailable");
        return;
    }

    if(!seader_temp_strings_ensure(seader, 3U)) {
        FURI_LOG_E(TAG, "Temp string allocation failed");
        return;
    }
    FuriString* fw_str = seader->temp_string1;
    FuriString* info_str = seader->temp_string2;
    FuriString* status_str = seader->temp_string3;

    furi_string_reset(fw_str);
    furi_string_reset(info_str);
    furi_string_reset(status_str);

    furi_string_cat_printf(fw_str, "FW %d.%d", seader->sam_version[0], seader->sam_version[1]);
    furi_string_set_str(info_str, seader->sam_key_label);
    furi_string_printf(
        status_str,
        "%s\n%s",
        seader_board_status_label(seader->board_status),
        seader->uhf_status_label);

    widget_add_button_element(
        seader->widget, GuiButtonTypeLeft, "Back", seader_scene_sam_info_widget_callback, seader);

    widget_add_string_element(
        widget, 64, 14, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(info_str));
    widget_add_text_box_element(
        widget, 5, 22, 118, 22, AlignCenter, AlignTop, furi_string_get_cstr(status_str), false);
    widget_add_string_element(
        widget, 64, 50, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(fw_str));

    // No need to free fw_str as it's reused from seader struct

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
}

bool seader_scene_sam_info_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(seader->scene_manager);
        } else if(event.event == SeaderCustomEventSamStatusUpdated) {
            seader_scene_sam_info_on_exit(context);
            seader_scene_sam_info_on_enter(context);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
    }
    return consumed;
}

void seader_scene_sam_info_on_exit(void* context) {
    Seader* seader = context;

    // Clear views
    if(seader->widget) {
        widget_reset(seader->widget);
    }
    seader_temp_strings_release(seader, 3U);
}
