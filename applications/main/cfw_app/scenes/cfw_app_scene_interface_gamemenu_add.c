#include "../cfw_app.h"

static bool
    gamemenu_fap_selector(FuriString* path, void* context, uint8_t** icon, FuriString* name) {
    CFWApp* app = context;
    return flipper_application_load_name_and_icon(path, app->storage, icon, name);
}

void cfw_app_scene_interface_gamemenu_add_on_enter(void* context) {
    CFWApp* app = context;
    const DialogsFileBrowserOptions options = {
        .extension = ".fap",
        .icon = &I_unknown_10px,
        .skip_assets = true,
        .hide_ext = true,
        .item_loader_callback = gamemenu_fap_selector,
        .item_loader_context = app,
        .base_path = EXT_PATH("apps"),
    };
    FuriString* path = furi_string_alloc_set_str(GAMEMENU_GAMES_PATH);
    if(dialog_file_browser_show(app->dialogs, path, path, &options)) {
        if(cfw_app_push_gamemenu_app(app, furi_string_get_cstr(path))) {
            app->gamemenu_app_index = CharList_size(app->gamemenu_app_labels) - 1;
            app->save_gamemenu_apps = true;
            app->gamemenu_source = GameMenuSourceCustom;
        }
    }
    furi_string_free(path);
    scene_manager_previous_scene(app->scene_manager);
}

bool cfw_app_scene_interface_gamemenu_add_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfw_app_scene_interface_gamemenu_add_on_exit(void* context) {
    UNUSED(context);
}
