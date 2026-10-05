#include "../k2_rfid_app.h"
#include "k2_scene.h"
#include <storage/storage.h>

void k2_scene_saved_spools_on_enter(void* context) {
    K2RfidApp* app = context;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, EXT_PATH("nfc/CFS"));
    furi_record_close(RECORD_STORAGE);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".nfc", NULL);
    browser_options.base_path = EXT_PATH("nfc");

    FuriString* result_path = furi_string_alloc();
    FuriString* initial_path = furi_string_alloc_set_str(EXT_PATH("nfc/CFS"));

    bool selected = dialog_file_browser_show(app->dialogs, result_path, initial_path, &browser_options);

    if (selected) {
        bool loaded = k2_worker_load_spool_from_nfc(furi_string_get_cstr(result_path), &app->last_spool);
        if (loaded && app->last_spool.valid) {
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, K2SceneTagInfo);
        } else {
            notification_message(app->notifications, &sequence_error);
            scene_manager_previous_scene(app->scene_manager);
        }
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }

    furi_string_free(result_path);
    furi_string_free(initial_path);
}

bool k2_scene_saved_spools_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void k2_scene_saved_spools_on_exit(void* context) {
    UNUSED(context);
}
