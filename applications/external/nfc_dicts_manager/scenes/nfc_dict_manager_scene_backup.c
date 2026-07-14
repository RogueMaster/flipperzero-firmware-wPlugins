#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_backup_on_enter(void* context) {
    NfcDictManager* app = context;

    nfc_dict_manager_create_directories(app->storage);

    app->backup_stage = 0;
    app->backup_success = false;

    popup_set_header(app->popup, "Saving...", 64, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Creating backup of\ndictionaries...", 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 0);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, NULL);

    furi_timer_start(app->backup_timer, 500);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewPopup);
}

bool nfc_dict_manager_scene_backup_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_dict_manager_scene_backup_on_exit(void* context) {
    NfcDictManager* app = context;
    furi_timer_stop(app->backup_timer);
    popup_reset(app->popup);
}
