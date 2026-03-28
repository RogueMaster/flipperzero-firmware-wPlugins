#include "../seader_i.h"
#include <gui/elements.h>

void seader_scene_sam_wrong_widget_callback(GuiButtonType result, InputType type, void* context) {
    Seader* seader = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seader->view_dispatcher, result);
    }
}

void seader_scene_sam_wrong_on_enter(void* context) {
    Seader* seader = context;
    Widget* widget = seader_get_widget(seader);
    if(!widget) {
        FURI_LOG_E("SeaderSceneSamWrong", "Widget view unavailable");
        return;
    }
    char atr_summary[48];

    if(!seader_temp_strings_ensure(seader, 1U)) {
        FURI_LOG_E("SeaderSceneSamWrong", "Temp string allocation failed");
        return;
    }
    seader_format_atr_summary(seader->ATR, seader->ATR_len, atr_summary, sizeof(atr_summary));
    furi_string_reset(seader->temp_string1);
    furi_string_printf(seader->temp_string1, "%s\nUse supported SAM", atr_summary);

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", seader_scene_sam_wrong_widget_callback, seader);
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Saved", seader_scene_sam_wrong_widget_callback, seader);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Retry", seader_scene_sam_wrong_widget_callback, seader);

    widget_add_string_element(
        widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "Unsupported SAM");
    widget_add_text_box_element(
        widget,
        8,
        21,
        112,
        20,
        AlignCenter,
        AlignTop,
        furi_string_get_cstr(seader->temp_string1),
        false);

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
}

bool seader_scene_sam_wrong_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            seader->board_status = SeaderBoardStatusRetryRequested;
            scene_manager_next_scene(seader->scene_manager, SeaderSceneStart);
            consumed = true;
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneFileSelect);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            scene_manager_stop(seader->scene_manager);
            view_dispatcher_stop(seader->view_dispatcher);
            consumed = true;
        } else if(event.event == SeaderWorkerEventSamPresent) {
            seader->board_status = SeaderBoardStatusReady;
            seader->sam_present_menu_guard_active = true;
            scene_manager_next_scene(seader->scene_manager, SeaderSceneSamPresent);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(seader->scene_manager);
        view_dispatcher_stop(seader->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void seader_scene_sam_wrong_on_exit(void* context) {
    Seader* seader = context;
    if(seader->widget) {
        widget_reset(seader->widget);
    }
    seader_temp_strings_release(seader, 1U);
}
