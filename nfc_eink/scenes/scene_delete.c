#include "../nfc_eink_app_i.h"

void nfc_eink_scene_delete_widget_callback(GuiButtonType result, InputType type, void* context) {
    NfcEinkApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_eink_scene_delete_on_enter(void* context) {
    NfcEinkApp* app = context;

    // Setup Custom Widget view
    FuriString* temp_str;
    temp_str = furi_string_alloc();

    furi_string_printf(temp_str, "\e#Delete %s?\e#", furi_string_get_cstr(app->file_name));
    widget_add_text_box_element(
        app->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, furi_string_get_cstr(temp_str), false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", nfc_eink_scene_delete_widget_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Delete", nfc_eink_scene_delete_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcEinkViewWidget);
}

bool nfc_eink_scene_delete_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            if(nfc_eink_screen_delete(furi_string_get_cstr(instance->file_path))) {
                scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneDeleteSuccess);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    instance->scene_manager, NfcEinkAppSceneStart);
            }
            consumed = true;
        }
    }
    return consumed;
}

void nfc_eink_scene_delete_on_exit(void* context) {
    NfcEinkApp* instance = context;

    widget_reset(instance->widget);
}
