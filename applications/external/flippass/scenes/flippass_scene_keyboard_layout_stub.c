#include "flippass_scene_keyboard_layout.h"

#include "../flippass.h"
#include "flippass_scene_status.h"

void flippass_scene_keyboard_layout_on_enter(void* context) {
    App* app = context;
    furi_assert(app);

    flippass_scene_status_show(
        app,
        "Typing Disabled",
        "Keyboard layout selection is disabled in this minimal-memory build.",
        app->keyboard_layout_return_scene);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
}

bool flippass_scene_keyboard_layout_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flippass_scene_keyboard_layout_on_exit(void* context) {
    UNUSED(context);
}
