#include "../seos_i.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneZeroKeys"

void seos_scene_zero_keys_popup_callback(void* context) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventViewExit);
}

void seos_scene_zero_keys_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = seos->popup;
    popup_set_header(popup, "NO KEYS", 64, 16, AlignCenter, AlignTop);
    popup_set_text(popup, "Using all zero keys", 64, 36, AlignCenter, AlignTop);
    popup_set_timeout(popup, 5 * 1000);
    popup_set_context(popup, seos);
    popup_set_callback(popup, seos_scene_zero_keys_popup_callback);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_zero_keys_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventViewExit) {
            scene_manager_next_scene(seos->scene_manager, SeosSceneMainMenu);
            consumed = true;
        }
    }

    return consumed;
}

void seos_scene_zero_keys_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    popup_reset(seos->popup);
}
