#include "../seos_i.h"
#include "seos_scene.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneWriteCardSuccess"

void seos_scene_write_error_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Seos* seos = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seos->view_dispatcher, result);
    }
}

void seos_scene_write_error_on_enter(void* context) {
    Seos* seos = context;
    Widget* widget = seos->widget;

    // Send notification
    notification_message(seos->notifications, &sequence_success);
    FuriString* primary_str = furi_string_alloc_set("Write Errror");
    FuriString* secondary_str = furi_string_alloc_set("Try again?");

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", seos_scene_write_error_widget_callback, seos);

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
    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewWidget);
}

bool seos_scene_write_error_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(seos->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(seos->scene_manager, SeosSceneSavedMenu);
        consumed = true;
    }
    return consumed;
}

void seos_scene_write_error_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    widget_reset(seos->widget);
}
