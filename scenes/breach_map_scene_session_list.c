#include "../breach_map_i.h"

static void breach_map_scene_session_list_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void breach_map_scene_session_list_on_enter(void* context) {
    BreachMapApp* app = context;
    Submenu* submenu = app->submenu;

    /* release previously cached names */
    for(size_t i = 0; i < app->session_names_count; i++) {
        furi_string_free(app->session_names[i]);
    }
    app->session_names_count =
        recon_storage_list_sessions(app->storage, app->session_names, RECON_MAX_SESSION_FILES);

    submenu_reset(submenu);
    submenu_set_header(submenu, "Open engagement");

    if(app->session_names_count == 0) {
        submenu_add_item(
            submenu, "(no saved sessions)", 0, breach_map_scene_session_list_callback, app);
    } else {
        for(size_t i = 0; i < app->session_names_count; i++) {
            submenu_add_item(
                submenu,
                furi_string_get_cstr(app->session_names[i]),
                i,
                breach_map_scene_session_list_callback,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_session_list_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(app->session_names_count == 0) {
            /* nothing to open */
        } else if(event.event < app->session_names_count) {
            const char* name = furi_string_get_cstr(app->session_names[event.event]);
            if(recon_storage_load_session(app->storage, app->session, name)) {
                strncpy(app->session_file, name, RECON_NAME_LEN - 1);
                app->session_file[RECON_NAME_LEN - 1] = '\0';
                scene_manager_next_scene(app->scene_manager, BreachMapSceneSessionMenu);
            }
        }
    }
    return consumed;
}

void breach_map_scene_session_list_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
