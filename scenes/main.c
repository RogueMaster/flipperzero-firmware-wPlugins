#include "../ble_spam.h"

void scene_main_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewMain);
}

bool scene_main_on_event(void* _ctx, SceneManagerEvent event) {
    // Stop advertising before leaving this scene (safety)
    if(event.type == SceneManagerEventTypeBack) {
        ble_spam_stop_adv(_ctx);
    }
    UNUSED(event);
    return false;
}

void scene_main_on_exit(void* _ctx) {
    UNUSED(_ctx);
}
