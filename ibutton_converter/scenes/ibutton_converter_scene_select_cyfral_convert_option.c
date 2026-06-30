#include "../ibutton_converter_i.h"
#include "ibutton_converter_scene.h"
#include "../utils/ibutton_converter_utils.h"

enum SubmenuIndex {
    SubmenuIndexC1,
    SubmenuIndexC2,
    SubmenuIndexC2Alt,
    SubmenuIndexC3,
    SubmenuIndexC4,
    SubmenuIndexC5,
    SubmenuIndexC6,
    SubmenuIndexC7,
};

void ibutton_converter_scene_select_cyfral_convert_option_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;
    Submenu* submenu = ibutton_converter->submenu;

    submenu_add_item(
        submenu, "C1", SubmenuIndexC1, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_add_item(
        submenu, "C2", SubmenuIndexC2, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_add_item(
        submenu,
        "C2 (Alt)",
        SubmenuIndexC2Alt,
        ibutton_converter_submenu_callback,
        ibutton_converter);

    submenu_add_item(
        submenu, "C3", SubmenuIndexC3, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_add_item(
        submenu, "C4", SubmenuIndexC4, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_add_item(
        submenu, "C5", SubmenuIndexC5, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_add_item(
        submenu, "C6", SubmenuIndexC6, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_add_item(
        submenu, "C7", SubmenuIndexC7, ibutton_converter_submenu_callback, ibutton_converter);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(
            ibutton_converter->scene_manager, iButtonConverterSceneSelectCyfralConvertOption));

    view_dispatcher_switch_to_view(
        ibutton_converter->view_dispatcher, iButtonConverterViewSubmenu);
}

bool ibutton_converter_scene_select_cyfral_convert_option_on_event(
    void* context,
    SceneManagerEvent event) {
    iButtonConverter* ibutton_converter = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            ibutton_converter->scene_manager,
            iButtonConverterSceneSelectCyfralConvertOption,
            event.event);
        consumed = true;

        // prepare input data
        iButtonEditableData cyfral_editable_data;
        ibutton_protocols_get_editable_data(
            ibutton_converter->protocols, ibutton_converter->source_key, &cyfral_editable_data);

        uint8_t cyfral_data[2];
        memcpy(cyfral_data, cyfral_editable_data.ptr, 2);

        // prepare output data
        iButtonKey* converted_key = ibutton_key_alloc(8);

        const iButtonProtocolId id =
            ibutton_protocols_get_id_by_name(ibutton_converter->protocols, "DS1990");

        ibutton_key_set_protocol_id(converted_key, id);

        iButtonEditableData dallas_editable_data;
        ibutton_protocols_get_editable_data(
            ibutton_converter->protocols, converted_key, &dallas_editable_data);

        if(event.event == SubmenuIndexC1) {
            cyfral_to_dallas_c1(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC2) {
            cyfral_to_dallas_c2(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC2Alt) {
            cyfral_to_dallas_c2_alt(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC3) {
            cyfral_to_dallas_c3(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC4) {
            cyfral_to_dallas_c4(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC5) {
            cyfral_to_dallas_c5(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC6) {
            cyfral_to_dallas_c6(cyfral_data, dallas_editable_data.ptr);
        } else if(event.event == SubmenuIndexC7) {
            cyfral_to_dallas_c7(cyfral_data, dallas_editable_data.ptr);
        }

        if(ibutton_converter->converted_key) {
            ibutton_key_free(ibutton_converter->converted_key);
        }
        ibutton_converter->converted_key = converted_key;

        scene_manager_next_scene(
            ibutton_converter->scene_manager, iButtonConverterSceneConvertedKeyMenu);
    }

    return consumed;
}

void ibutton_converter_scene_select_cyfral_convert_option_on_exit(void* context) {
    iButtonConverter* ibutton_converter = context;
    submenu_reset(ibutton_converter->submenu);
}
