#include "../passy_i.h"

void passy_scene_delete_widget_callback(GuiButtonType result, InputType type, void* context) {
    Passy* passy = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, result);
    }
}

void passy_scene_delete_on_enter(void* context) {
    Passy* passy = context;

    // Setup Custom Widget view
    char temp_str[64];
    snprintf(temp_str, sizeof(temp_str), "\e#Delete MRZ info?\e#");
    widget_add_text_box_element(
        passy->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, temp_str, false);
    widget_add_button_element(
        passy->widget, GuiButtonTypeLeft, "Back", passy_scene_delete_widget_callback, passy);
    widget_add_button_element(
        passy->widget, GuiButtonTypeRight, "Delete", passy_scene_delete_widget_callback, passy);

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewWidget);
}

bool passy_scene_delete_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            return scene_manager_previous_scene(passy->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            if(passy_delete_mrz_info(passy)) {
                scene_manager_next_scene(passy->scene_manager, PassySceneDeleteSuccess);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    passy->scene_manager, PassySceneMainMenu);
            }
            consumed = true;
        }
    }
    return consumed;
}

void passy_scene_delete_on_exit(void* context) {
    Passy* passy = context;

    widget_reset(passy->widget);
}
