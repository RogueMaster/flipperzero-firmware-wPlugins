#include "../weebo_i.h"
#include <dolphin/dolphin.h>
#include "../acknowledgements.h"

void weebo_scene_acknowledgements_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    Weebo* weebo = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(weebo->view_dispatcher, result);
    }
}

void weebo_scene_acknowledgements_on_enter(void* context) {
    Weebo* weebo = context;

    furi_string_reset(weebo->text_box_store);

    FuriString* str = weebo->text_box_store;
    furi_string_cat_printf(str, "%s\n", acknowledgements_text);

    text_box_set_font(weebo->text_box, TextBoxFontText);
    text_box_set_text(weebo->text_box, furi_string_get_cstr(weebo->text_box_store));
    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewTextBox);
}

bool weebo_scene_acknowledgements_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(weebo->scene_manager);
        }
    }
    return consumed;
}

void weebo_scene_acknowledgements_on_exit(void* context) {
    Weebo* weebo = context;

    // Clear views
    text_box_reset(weebo->text_box);
}
