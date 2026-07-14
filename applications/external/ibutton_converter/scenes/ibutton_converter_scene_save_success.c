#include "../ibutton_converter_i.h"
#include <dolphin/dolphin.h>

static void ibutton_converter_scene_save_success_popup_callback(void* context) {
    iButtonConverter* ibutton_converter = context;
    view_dispatcher_send_custom_event(
        ibutton_converter->view_dispatcher, iButtonConverterCustomEventBack);
}

void ibutton_converter_scene_save_success_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;
    Popup* popup = ibutton_converter->popup;

    dolphin_deed(DolphinDeedIbuttonAdd);

    popup_set_icon(popup, 0, 9, &I_DolphinSuccess_91x55);
    popup_set_header(popup, "Saved", 63, 19, AlignLeft, AlignBottom);
    popup_set_callback(popup, ibutton_converter_scene_save_success_popup_callback);
    popup_set_context(popup, ibutton_converter);
    popup_set_timeout(popup, 1500);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(ibutton_converter->view_dispatcher, iButtonConverterViewPopup);
}

bool ibutton_converter_scene_save_success_on_event(void* context, SceneManagerEvent event) {
    iButtonConverter* ibutton_converter = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == iButtonConverterCustomEventBack) {
            const uint32_t possible_scenes[] = {
                iButtonConverterSceneSelectMetakomConvertOption,
                iButtonConverterSceneSelectCyfralConvertOption};
            scene_manager_search_and_switch_to_previous_scene_one_of(
                ibutton_converter->scene_manager, possible_scenes, COUNT_OF(possible_scenes));
        }
    }

    return consumed;
}

void ibutton_converter_scene_save_success_on_exit(void* context) {
    iButtonConverter* ibutton_converter = context;
    Popup* popup = ibutton_converter->popup;

    popup_reset(popup);
}
