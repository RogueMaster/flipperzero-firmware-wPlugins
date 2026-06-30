#include "../ibutton_converter_i.h"
#include "ibutton_converter_scene.h"
#include "../utils/ibutton_converter_utils.h"

enum SubmenuIndex {
    SubmenuIndexDirect,
    SubmenuIndexReversed,
};

void ibutton_converter_scene_select_metakom_convert_option_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;
    Submenu* submenu = ibutton_converter->submenu;

    submenu_add_item(
        submenu,
        "Direct",
        SubmenuIndexDirect,
        ibutton_converter_submenu_callback,
        ibutton_converter);

    submenu_add_item(
        submenu,
        "Reversed",
        SubmenuIndexReversed,
        ibutton_converter_submenu_callback,
        ibutton_converter);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(
            ibutton_converter->scene_manager, iButtonConverterSceneSelectMetakomConvertOption));

    view_dispatcher_switch_to_view(
        ibutton_converter->view_dispatcher, iButtonConverterViewSubmenu);
}

bool ibutton_converter_scene_select_metakom_convert_option_on_event(
    void* context,
    SceneManagerEvent event) {
    iButtonConverter* ibutton_converter = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            ibutton_converter->scene_manager,
            iButtonConverterSceneSelectMetakomConvertOption,
            event.event);
        consumed = true;

        // prepare input data
        iButtonEditableData metakom_editable_data;
        ibutton_protocols_get_editable_data(
            ibutton_converter->protocols, ibutton_converter->source_key, &metakom_editable_data);

        uint8_t metakom_data[4];
        memcpy(metakom_data, metakom_editable_data.ptr, 4);

        // prepare output data
        iButtonKey* converted_key = ibutton_key_alloc(8);

        const iButtonProtocolId id =
            ibutton_protocols_get_id_by_name(ibutton_converter->protocols, "DS1990");

        ibutton_key_set_protocol_id(converted_key, id);

        iButtonEditableData dallas_editable_data;
        ibutton_protocols_get_editable_data(
            ibutton_converter->protocols, converted_key, &dallas_editable_data);

        if(event.event == SubmenuIndexDirect) {
            metakom_to_dallas(metakom_data, dallas_editable_data.ptr, 0);
        } else if(event.event == SubmenuIndexReversed) {
            metakom_to_dallas(metakom_data, dallas_editable_data.ptr, 1);
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

void ibutton_converter_scene_select_metakom_convert_option_on_exit(void* context) {
    iButtonConverter* ibutton_converter = context;
    submenu_reset(ibutton_converter->submenu);
}
