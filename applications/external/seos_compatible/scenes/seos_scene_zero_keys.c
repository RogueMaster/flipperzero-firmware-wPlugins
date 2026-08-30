#include "seos_scene_popup.h"
#include <dolphin/dolphin.h>

void seos_scene_zero_keys_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcRead);

    const SeosScenePopup config = {
        .header = "NO KEYS",
        .header_x = 64,
        .header_y = 16,
        .text = "Using all zero keys",
        .text_x = 64,
        .text_y = 36,
        .horizontal = AlignCenter,
        .vertical = AlignTop,
        .timeout_ms = 5 * 1000,
    };
    seos_scene_popup_enter(seos, &config);
}

/* This one goes on to the main menu rather than back to it, so it does not
 * use the shared event handler. */
bool seos_scene_zero_keys_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == SeosCustomEventViewExit) {
        scene_manager_next_scene(seos->scene_manager, SeosSceneMainMenu);
        return true;
    }
    return false;
}

void seos_scene_zero_keys_on_exit(void* context) {
    seos_scene_popup_exit(context);
}
