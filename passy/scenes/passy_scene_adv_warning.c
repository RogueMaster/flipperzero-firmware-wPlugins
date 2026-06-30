#include "../passy_i.h"

#define TAG "PassySceneReadCardSuccess"

void passy_scene_adv_warning_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Passy* passy = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, result);
    }
}

void passy_scene_adv_warning_on_enter(void* context) {
    Passy* passy = context;
    Widget* widget = passy->widget;

    FuriString* first_str = furi_string_alloc_set("These DG may require");
    FuriString* second_str = furi_string_alloc_set("advanced authentication.\n");
    FuriString* third_str = furi_string_alloc_set("Do not expect them to work.\n");
    FuriString* fourth_str = furi_string_alloc_set("Do not open issues for them.\n");

    widget_add_string_element(
        widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(first_str));
    widget_add_string_element(
        widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(second_str));
    widget_add_string_element(
        widget, 0, 32, AlignLeft, AlignCenter, FontSecondary, furi_string_get_cstr(third_str));
    widget_add_string_element(
        widget, 0, 44, AlignLeft, AlignCenter, FontSecondary, furi_string_get_cstr(fourth_str));

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "OK", passy_scene_adv_warning_widget_callback, passy);

    furi_string_free(first_str);
    furi_string_free(second_str);
    furi_string_free(third_str);
    furi_string_free(fourth_str);

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewWidget);
}

bool passy_scene_adv_warning_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(passy->scene_manager);
        } else if(event.event == GuiButtonTypeCenter) {
            passy->read_type = PassyReadCOM;
            scene_manager_next_scene(passy->scene_manager, PassySceneRead);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(passy->scene_manager);
    }
    return consumed;
}

void passy_scene_adv_warning_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    widget_reset(passy->widget);
}
