#include "../seos_i.h"

void seos_scene_delete_widget_callback(GuiButtonType result, InputType type, void* context) {
    Seos* seos = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seos->view_dispatcher, result);
    }
}

void seos_scene_delete_on_enter(void* context) {
    Seos* seos = context;
    SeosCredential* seos_credential = seos->credential;

    // Setup Custom Widget view
    char temp_str[141];
    snprintf(temp_str, sizeof(temp_str), "\e#Delete %s?\e#", seos_credential->name);
    widget_add_text_box_element(
        seos->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, temp_str, false);
    widget_add_button_element(
        seos->widget, GuiButtonTypeLeft, "Back", seos_scene_delete_widget_callback, seos);
    widget_add_button_element(
        seos->widget, GuiButtonTypeRight, "Delete", seos_scene_delete_widget_callback, seos);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewWidget);
}

bool seos_scene_delete_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    SeosCredential* seos_credential = seos->credential;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            return scene_manager_previous_scene(seos->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            if(seos_credential_delete(seos_credential, true)) {
                scene_manager_next_scene(seos->scene_manager, SeosSceneDeleteSuccess);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    seos->scene_manager, SeosSceneMainMenu);
            }
            consumed = true;
        }
    }
    return consumed;
}

void seos_scene_delete_on_exit(void* context) {
    Seos* seos = context;

    widget_reset(seos->widget);
}
