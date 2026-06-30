#include "../seader_i.h"
#include <dolphin/dolphin.h>

void seader_scene_read_config_card_success_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Seader* seader = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seader->view_dispatcher, result);
    }
}

void seader_scene_read_config_card_success_on_enter(void* context) {
    Seader* seader = context;
    Widget* widget = seader_get_widget(seader);
    if(!widget) {
        FURI_LOG_E("SeaderSceneReadConfigCardSuccess", "Widget view unavailable");
        return;
    }

    FuriString* config_card_str = furi_string_alloc();

    dolphin_deed(DolphinDeedNfcReadSuccess);

    // Send notification
    notification_message(seader->notifications, &sequence_success);

    furi_string_set(config_card_str, "Config card read complete");

    widget_add_string_element(
        widget,
        64,
        5,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(config_card_str));
    furi_string_free(config_card_str);

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
}

bool seader_scene_read_config_card_success_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }
    return consumed;
}

void seader_scene_read_config_card_success_on_exit(void* context) {
    Seader* seader = context;

    // Clear view
    if(seader->widget) {
        widget_reset(seader->widget);
    }
}
