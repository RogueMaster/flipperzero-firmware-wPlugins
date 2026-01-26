#include "../uk_mbirth_sonicare.h"
#include <dolphin/dolphin.h>

void sonicare_scene_read_on_enter(void* context) {
    Sonicare* app = context;
    UNUSED(app);
}

bool sonicare_scene_read_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    UNUSED(app);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void sonicare_scene_read_on_exit(void* context) {
    Sonicare* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
