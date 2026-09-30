#include "../k2_rfid_app.h"
#include "k2_scene.h"

void k2_scene_save_on_enter(void* context) {
    K2RfidApp* app = context;
    Popup* popup = app->popup;

    /* Ensure valid spool configuration */
    if (app->config.material_id[0] == '\0') {
        k2_rfid_app_set_default_config(app);
    } else {
        k2_rfid_app_sync_config(app);
    }

    char saved_path[128] = {0};
    bool ok = k2_worker_save_spool_to_nfc(&app->config, NULL, saved_path, sizeof(saved_path));

    popup_reset(popup);
    if (ok) {
        /* Extract file name from path */
        const char* fname = strrchr(saved_path, '/');
        fname = fname ? (fname + 1) : saved_path;

        FuriString* str = furi_string_alloc();
        furi_string_printf(str, "Saved to /ext/nfc/CFS/:\n%s\n\nReady to emulate!", fname);

        popup_set_header(popup, "Spool Saved!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, furi_string_get_cstr(str), 64, 25, AlignCenter, AlignTop);
        furi_string_free(str);

        notification_message(app->notifications, &sequence_success);
    } else {
        popup_set_header(popup, "Save Failed!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "Could not write to SD card.\nCheck SD card status.", 64, 25, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_error);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewPopup);
}

bool k2_scene_save_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void k2_scene_save_on_exit(void* context) {
    K2RfidApp* app = context;
    popup_reset(app->popup);
}
