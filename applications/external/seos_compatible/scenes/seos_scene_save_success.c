#include "seos_scene_popup.h"
#include <dolphin/dolphin.h>
#include <seos_icons.h>

void seos_scene_save_success_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcSave);

    const SeosScenePopup config = {
        .icon = &I_DolphinNice_96x59,
        .icon_x = 32,
        .icon_y = 5,
        .header = "Saved!",
        .header_x = 13,
        .header_y = 22,
        .horizontal = AlignLeft,
        .vertical = AlignBottom,
        .timeout_ms = 1500,
    };
    seos_scene_popup_enter(seos, &config);
}

bool seos_scene_save_success_on_event(void* context, SceneManagerEvent event) {
    return seos_scene_popup_event(context, event, SeosSceneMainMenu);
}

void seos_scene_save_success_on_exit(void* context) {
    seos_scene_popup_exit(context);
}
