#include "../clippy_app_i.h"
#include "core/core_defines.h"

void clippy_scene_main_button_callback(InputKey key, void* context) {
    furi_assert(context);
    ClippyApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, key);
}

void clippy_scene_copy_paste_sel_on_enter(void* context) {
    ClippyApp* app = context;

    clippy_main_view_set_button_callback(app->main_view, clippy_scene_main_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ClippyAppViewCopyPasteSelection);
}

bool clippy_scene_copy_paste_sel_on_event(void* context, SceneManagerEvent event) {
    ClippyApp* app = context;
    UNUSED(app);
    UNUSED(event);

    return false;
}

void clippy_scene_copy_paste_sel_on_exit(void* context) {
    ClippyApp* app = context;
    UNUSED(app);
}
