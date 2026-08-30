#include "seos_scene_popup.h"
#include <seos_icons.h>

void seos_scene_delete_success_on_enter(void* context) {
    Seos* seos = context;

    const SeosScenePopup config = {
        .icon = &I_DolphinMafia_115x62,
        .icon_x = 0,
        .icon_y = 2,
        .header = "Deleted",
        .header_x = 83,
        .header_y = 19,
        .horizontal = AlignLeft,
        .vertical = AlignBottom,
        .timeout_ms = 1500,
    };
    seos_scene_popup_enter(seos, &config);
}

bool seos_scene_delete_success_on_event(void* context, SceneManagerEvent event) {
    return seos_scene_popup_event(context, event, SeosSceneMainMenu);
}

void seos_scene_delete_success_on_exit(void* context) {
    seos_scene_popup_exit(context);
}
