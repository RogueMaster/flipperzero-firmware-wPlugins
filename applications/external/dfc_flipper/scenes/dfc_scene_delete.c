#include "../dfc_i.h"
#include "../dfc_credential_storage.h"

void dfc_scene_delete_widget_callback(GuiButtonType result, InputType type, void* context) {
    Dfc* dfc = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(dfc->view_dispatcher, result);
    }
}

void dfc_scene_delete_on_enter(void* context) {
    Dfc* dfc = context;
    DfcCredential* dfc_credential = dfc->credential;

    // Setup Custom Widget view
    char temp_str[141];
    snprintf(temp_str, sizeof(temp_str), "\e#Delete %s?\e#", dfc_credential->name);
    widget_add_text_box_element(
        dfc->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, temp_str, false);
    widget_add_button_element(
        dfc->widget, GuiButtonTypeLeft, "Back", dfc_scene_delete_widget_callback, dfc);
    widget_add_button_element(
        dfc->widget, GuiButtonTypeRight, "Delete", dfc_scene_delete_widget_callback, dfc);

    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewWidget);
}

bool dfc_scene_delete_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    DfcCredential* dfc_credential = dfc->credential;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            return scene_manager_previous_scene(dfc->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            if(dfc_credential_delete(dfc_credential, true)) {
                scene_manager_next_scene(dfc->scene_manager, DfcSceneDeleteSuccess);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    dfc->scene_manager, DfcSceneMainMenu);
            }
            consumed = true;
        }
    }
    return consumed;
}

void dfc_scene_delete_on_exit(void* context) {
    Dfc* dfc = context;

    widget_reset(dfc->widget);
}
