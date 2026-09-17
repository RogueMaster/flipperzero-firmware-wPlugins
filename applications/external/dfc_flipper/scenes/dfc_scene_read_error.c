#include "../dfc_i.h"
#include <dolphin/dolphin.h>

#define TAG "DfcSceneReadCardSuccess"

void dfc_scene_read_error_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Dfc* dfc = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(dfc->view_dispatcher, result);
    }
}

void dfc_scene_read_error_on_enter(void* context) {
    Dfc* dfc = context;
    Widget* widget = dfc->widget;

    // Send notification
    notification_message(dfc->notifications, &sequence_success);
    FuriString* primary_str = furi_string_alloc_set("Read Errror");
    FuriString* secondary_str = furi_string_alloc_set("Try again?");

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", dfc_scene_read_error_widget_callback, dfc);

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(primary_str));

    widget_add_string_element(
        widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(secondary_str));

    furi_string_free(primary_str);
    furi_string_free(secondary_str);
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewWidget);
}

bool dfc_scene_read_error_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(dfc->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void dfc_scene_read_error_on_exit(void* context) {
    Dfc* dfc = context;

    // Clear view
    widget_reset(dfc->widget);
}
