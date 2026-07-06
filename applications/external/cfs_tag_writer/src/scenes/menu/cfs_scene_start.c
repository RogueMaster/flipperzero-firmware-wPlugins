#include "src/core/cfs_app_i.h"

// Ids are based off CfsCustomEventMenuBase (cfs_app_i.h), which sits past the
// worker-event range so a stray worker event queued during teardown can't alias
// a menu action (e.g. NotPresent=2 -> Erase).
typedef enum {
    StartItemRead = CfsCustomEventMenuBase,
    StartItemWrite,
    StartItemErase,
    StartItemSaved,
    StartItemSettings,
    StartItemHelp,
    StartItemAbout,
} StartItem;

void cfs_scene_start_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "CFS Tag Writer");
    submenu_add_item(submenu, "Read Tag", StartItemRead, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Write Tag", StartItemWrite, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Erase Tag", StartItemErase, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Saved Tags", StartItemSaved, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Settings", StartItemSettings, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Help", StartItemHelp, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "About", StartItemAbout, cfs_submenu_send_index, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, CfsSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_start_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CfsSceneStart, event.event);
        switch(event.event) {
        case StartItemRead:
            scene_manager_next_scene(app->scene_manager, CfsSceneRead);
            return true;
        case StartItemWrite:
            // Fresh from-scratch write: seed defaults + configured brand.
            cfs_data_set_defaults(&app->pending);
            strncpy(
                app->pending.vendor,
                cfs_brand_to_vendor(app->settings.default_brand),
                sizeof(app->pending.vendor));
            app->dual_tag = false;
            app->write_exact = false;
            app->has_source_serial = false;
            app->pending_weight_index = 0;
            scene_manager_next_scene(app->scene_manager, CfsSceneWriteBrand);
            return true;
        case StartItemErase:
            scene_manager_next_scene(app->scene_manager, CfsSceneEraseConfirm);
            return true;
        case StartItemSaved:
            scene_manager_next_scene(app->scene_manager, CfsSceneSavedTags);
            return true;
        case StartItemSettings:
            scene_manager_next_scene(app->scene_manager, CfsSceneSettings);
            return true;
        case StartItemHelp:
            scene_manager_next_scene(app->scene_manager, CfsSceneHelp);
            return true;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, CfsSceneAbout);
            return true;
        default:
            break;
        }
    }
    return false;
}

void cfs_scene_start_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}
