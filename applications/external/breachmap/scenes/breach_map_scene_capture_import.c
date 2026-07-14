#include "../breach_map_i.h"
#include "../modules/capture_meta.h"

static void breach_map_scene_capture_import_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void scan_dir(BreachMapApp* app, const char* dir, const char* ext) {
    File* d = storage_file_alloc(app->storage);
    char name[128];
    FileInfo info;
    if(storage_dir_open(d, dir)) {
        while(app->import_count < RECON_MAX_IMPORT &&
              storage_dir_read(d, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            size_t ln = strlen(name), le = strlen(ext);
            if(ln < le || strcasecmp(name + ln - le, ext) != 0) continue;
            FuriString* full = furi_string_alloc();
            furi_string_printf(full, "%s/%s", dir, name);
            app->import_paths[app->import_count++] = full;
        }
    }
    storage_dir_close(d);
    storage_file_free(d);
}

void breach_map_scene_capture_import_on_enter(void* context) {
    BreachMapApp* app = context;
    Submenu* submenu = app->submenu;

    for(size_t i = 0; i < app->import_count; i++)
        furi_string_free(app->import_paths[i]);
    app->import_count = 0;

    scan_dir(app, EXT_PATH("subghz"), ".sub");
    scan_dir(app, EXT_PATH("nfc"), ".nfc");

    submenu_reset(submenu);
    submenu_set_header(submenu, "Import capture");
    if(app->import_count == 0) {
        submenu_add_item(
            submenu, "(no captures found)", 0, breach_map_scene_capture_import_callback, app);
    } else {
        for(size_t i = 0; i < app->import_count; i++) {
            const char* full = furi_string_get_cstr(app->import_paths[i]);
            const char* base = strrchr(full, '/');
            submenu_add_item(
                submenu, base ? base + 1 : full, i, breach_map_scene_capture_import_callback, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_capture_import_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(app->import_count == 0 || event.event >= app->import_count) return true;

        const char* path = furi_string_get_cstr(app->import_paths[event.event]);
        uint16_t asset_id = (app->selected_asset < app->session->asset_count) ?
                                app->session->assets[app->selected_asset].id :
                                RECON_INVALID_INDEX;
        const char* base = strrchr(path, '/');
        uint16_t idx = asset_manager_add_evidence(
            app->session,
            asset_id,
            capture_meta_type_from_path(path),
            base ? base + 1 : path,
            path);
        if(idx != RECON_INVALID_INDEX) {
            capture_meta_extract(
                app->storage, path, app->session->evidence[idx].info, RECON_NAME_LEN);
            notification_message(app->notifications, &sequence_success);
        }
        scene_manager_previous_scene(app->scene_manager);
    }
    return consumed;
}

void breach_map_scene_capture_import_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
