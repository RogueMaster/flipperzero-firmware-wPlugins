#include "../../rf_studio_i.h"

enum {
    StartMyRemotes,
    StartFavorites,
    StartNewRemote,
    StartCapture,
    StartSignals,
    StartImport,
    StartSettings,
    StartDiagnostics,
};

void rfs_scene_start_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "RF Studio");
    submenu_add_item(submenu, "My Remotes", StartMyRemotes, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Favorites", StartFavorites, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "New Remote", StartNewRemote, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Capture Signal", StartCapture, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Signal Library", StartSignals, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Import Remote", StartImport, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Settings", StartSettings, rfs_app_submenu_callback, app);
    submenu_add_item(submenu, "Diagnostics", StartDiagnostics, rfs_app_submenu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_start_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneStart, index);

    if(!app->sd_ready && index != StartSettings && index != StartDiagnostics) {
        app->sd_ready = rfs_storage_init(app->storage);
        if(app->sd_ready) {
            rfs_signal_index_load(app->storage, &app->signals);
        } else {
            rfs_app_message(
                app, "No SD card", "Insert a microSD card to\nuse the library.", RfsSceneNum);
            return true;
        }
    }

    switch(index) {
    case StartMyRemotes:
    case StartFavorites:
        app->favorites_only = index == StartFavorites;
        app->search[0] = '\0';
        scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteList, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneRemoteList);
        break;
    case StartNewRemote:
        app->text_purpose = RfsTextRemoteNew;
        app->text_buffer[0] = '\0';
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
        break;
    case StartCapture:
        rfs_app_start_library_capture(app);
        break;
    case StartSignals:
        app->signal_list_mode = RfsSignalListBrowse;
        scene_manager_set_scene_state(app->scene_manager, RfsSceneSignalList, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneSignalList);
        break;
    case StartImport:
        app->import_kind = RfsImportRemote;
        scene_manager_next_scene(app->scene_manager, RfsSceneFileImport);
        break;
    case StartSettings:
        scene_manager_next_scene(app->scene_manager, RfsSceneSettings);
        break;
    case StartDiagnostics:
        app->info_kind = RfsInfoDiagnostics;
        scene_manager_next_scene(app->scene_manager, RfsSceneInfo);
        break;
    default:
        break;
    }
    return true;
}

void rfs_scene_start_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
