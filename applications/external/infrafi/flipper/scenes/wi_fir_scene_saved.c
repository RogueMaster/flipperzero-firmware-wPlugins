#include "../wi_fir.h"

static void wi_fir_scene_saved_callback(void* context, uint32_t index) {
    WiFirApp* app = context;
    scene_manager_set_scene_state(app->scene_manager, WiFirSceneSaved, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventSavedSelected);
}

void wi_fir_scene_saved_on_enter(void* context) {
    WiFirApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Saved Networks");

    /* Scan SD card for saved credentials */
    app->saved_count = wfr_storage_list(
        app->storage, app->saved_ssids, app->saved_files, WFR_SAVED_MAX);

    if(app->saved_count == 0) {
        submenu_add_item(app->submenu, "(no saved networks)", 0, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->saved_count; i++) {
            submenu_add_item(
                app->submenu, app->saved_ssids[i], i, wi_fir_scene_saved_callback, app);
        }
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WiFirSceneSaved));

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewSubmenu);
}

bool wi_fir_scene_saved_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventSavedSelected) {
            uint32_t index =
                scene_manager_get_scene_state(app->scene_manager, WiFirSceneSaved);

            if(index < app->saved_count) {
                /* Build full path and load credentials */
                char path[256];
                snprintf(
                    path,
                    sizeof(path),
                    "%s/%s",
                    WFR_SAVE_DIR,
                    app->saved_files[index]);

                WfrWifiCreds creds;
                if(wfr_storage_load(app->storage, path, &creds)) {
                    strncpy(app->ssid, creds.ssid, WFR_SSID_MAX_LEN);
                    strncpy(app->password, creds.password, WFR_PASS_MAX_LEN);
                    app->security_type = creds.security;
                    app->hidden = creds.hidden;
                    strncpy(
                        app->selected_saved_file,
                        app->saved_files[index],
                        WFR_FILENAME_MAX - 1);

                    scene_manager_next_scene(app->scene_manager, WiFirSceneConfirm);
                    consumed = true;
                }
            }
        }
    }

    return consumed;
}

void wi_fir_scene_saved_on_exit(void* context) {
    WiFirApp* app = context;
    submenu_reset(app->submenu);
}
