#include "../dfc_i.h"
#include <dolphin/dolphin.h>

void dfc_scene_save_success_popup_callback(void* context) {
    Dfc* dfc = context;
    view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventViewExit);
}

void dfc_scene_save_success_on_enter(void* context) {
    Dfc* dfc = context;
    dolphin_deed(DolphinDeedNfcSave);

    // Setup view
    Popup* popup = dfc->popup;
    popup_set_icon(popup, 32, 5, &I_DolphinNice_96x59);
    popup_set_header(popup, "Saved!", 13, 22, AlignLeft, AlignBottom);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, dfc);
    popup_set_callback(popup, dfc_scene_save_success_popup_callback);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
}

bool dfc_scene_save_success_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DfcCustomEventViewExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                dfc->scene_manager, DfcSceneMainMenu);
        }
    }
    return consumed;
}

void dfc_scene_save_success_on_exit(void* context) {
    Dfc* dfc = context;

    // Clear view
    popup_reset(dfc->popup);
}
