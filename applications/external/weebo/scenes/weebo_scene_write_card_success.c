#include "../weebo_i.h"
#include <dolphin/dolphin.h>

void weebo_scene_write_card_success_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Weebo* weebo = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(weebo->view_dispatcher, result);
    }
}

void weebo_scene_write_card_success_on_enter(void* context) {
    Weebo* weebo = context;
    Widget* widget = weebo->widget;
    FuriString* str = furi_string_alloc_set("Write Success!");

    dolphin_deed(DolphinDeedNfcReadSuccess);

    // Send notification
    notification_message(weebo->notifications, &sequence_success);

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(str));

    furi_string_free(str);

    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewWidget);
}

bool weebo_scene_write_card_success_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    // Jump back to main menu
    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            weebo->scene_manager, WeeboSceneMainMenu);
    }
    return consumed;
}

void weebo_scene_write_card_success_on_exit(void* context) {
    Weebo* weebo = context;

    // Clear view
    widget_reset(weebo->widget);
}
