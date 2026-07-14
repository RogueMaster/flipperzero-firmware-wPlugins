#include "../ibutton_converter_i.h"

void ibutton_converter_scene_select_key_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;

    ibutton_converter_reset_key(ibutton_converter);

    if(ibutton_converter_select_and_load_key(ibutton_converter)) {
        // detect loaded key type
        const iButtonProtocolId metakom_id =
            ibutton_protocols_get_id_by_name(ibutton_converter->protocols, "Metakom");
        const iButtonProtocolId cyfral_id =
            ibutton_protocols_get_id_by_name(ibutton_converter->protocols, "Cyfral");

        const iButtonProtocolId loaded_key_protocol_id =
            ibutton_key_get_protocol_id(ibutton_converter->source_key);

        if(loaded_key_protocol_id == metakom_id) {
            scene_manager_next_scene(
                ibutton_converter->scene_manager, iButtonConverterSceneSelectMetakomConvertOption);
        } else if(loaded_key_protocol_id == cyfral_id) {
            scene_manager_next_scene(
                ibutton_converter->scene_manager, iButtonConverterSceneSelectCyfralConvertOption);
        } else {
            scene_manager_set_scene_state(
                ibutton_converter->scene_manager, iButtonConverterSceneError, 0);
            scene_manager_next_scene(ibutton_converter->scene_manager, iButtonConverterSceneError);
        }

    } else {
        scene_manager_search_and_switch_to_previous_scene(
            ibutton_converter->scene_manager, iButtonConverterSceneStart);
    }
}

bool ibutton_converter_scene_select_key_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ibutton_converter_scene_select_key_on_exit(void* context) {
    UNUSED(context);
}
