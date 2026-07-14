#include "../flip_aic.h"
#include "flip_aic_scene.h"

void flip_aic_scene_saved_file_browser_callback(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    const char* path = furi_string_get_cstr(app->file_browser_result_path);
    if(nfc_device_load(app->nfc_device, path)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

void flip_aic_scene_saved_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    file_browser_set_callback(app->file_browser, flip_aic_scene_saved_file_browser_callback, app);
    file_browser_start(app->file_browser, app->file_browser_result_path);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewFileBrowser);
}

bool flip_aic_scene_saved_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_next_scene(app->scene_manager, FlipAICSceneDisplay);
        return true;
    }

    return false;
}

void flip_aic_scene_saved_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    file_browser_stop(app->file_browser);
}
