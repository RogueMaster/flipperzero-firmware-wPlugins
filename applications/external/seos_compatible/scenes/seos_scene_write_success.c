#include "../seos_i.h"
#include "seos_scene.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneWriteCardSuccess"

void seos_scene_write_popup_callback(void* context) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventViewExit);
}

void seos_scene_write_success_on_enter(void* context) {
    Seos* seos = context;

    dolphin_deed(DolphinDeedNfcReadSuccess);

    // Setup view
    Popup* popup = seos->popup;
    popup_set_icon(popup, 32, 5, &I_DolphinNice_96x59);
    popup_set_header(popup, "SIO Written!", 4, 12, AlignLeft, AlignBottom);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, seos);
    popup_set_callback(popup, seos_scene_write_popup_callback);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_write_success_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventViewExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                seos->scene_manager, SeosSceneSavedMenu);
        }
    }
    return consumed;
}

void seos_scene_write_success_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    popup_reset(seos->popup);
}
