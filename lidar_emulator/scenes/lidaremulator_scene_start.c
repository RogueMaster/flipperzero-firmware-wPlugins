#include "../lidaremulator_app_i.h"

enum SubmenuIndex {
    SubmenuIndexPredefinedGUNs,
    SubmenuIndexSettings,
};

static void lidaremulator_scene_start_submenu_callback(void* context, uint32_t index) {
    LidarEmulatorApp* lidaremulator = context;
    furi_check(lidaremulator);

    view_dispatcher_send_custom_event(lidaremulator->view_dispatcher, index);
}

void lidaremulator_scene_start_on_enter(void* context) {
    LidarEmulatorApp* lidaremulator = context;
    furi_check(lidaremulator);

    Submenu* submenu = lidaremulator->submenu;
    furi_check(submenu);

    SceneManager* scene_manager = lidaremulator->scene_manager;
    furi_check(scene_manager);

    submenu_add_item(submenu, "Predefined GUNs", SubmenuIndexPredefinedGUNs, lidaremulator_scene_start_submenu_callback, lidaremulator);
    submenu_add_item(submenu, "Settings", SubmenuIndexSettings, lidaremulator_scene_start_submenu_callback, lidaremulator);

    uint32_t submenu_index = scene_manager_get_scene_state(scene_manager, LidarEmulatorSceneStart);
    submenu_set_selected_item(submenu, submenu_index);
    scene_manager_set_scene_state(scene_manager, LidarEmulatorSceneStart, SubmenuIndexPredefinedGUNs);

    view_dispatcher_switch_to_view(lidaremulator->view_dispatcher, LidarEmulatorViewSubmenu);
}

bool lidaremulator_scene_start_on_event(void* context, SceneManagerEvent event) {
    LidarEmulatorApp* lidaremulator = context;
    furi_check(lidaremulator);

    SceneManager* scene_manager = lidaremulator->scene_manager;
    furi_check(scene_manager);

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        scene_manager_set_scene_state(scene_manager, LidarEmulatorSceneStart, submenu_index);
        if(submenu_index == SubmenuIndexSettings) {
            scene_manager_next_scene(scene_manager, LidarEmulatorSceneSettings);
        } else if(submenu_index == SubmenuIndexPredefinedGUNs) {
            scene_manager_next_scene(scene_manager, LidarEmulatorScenePredefinedGUNs);
        } 

        consumed = true;
    }

    return consumed;
}

void lidaremulator_scene_start_on_exit(void* context) {
    LidarEmulatorApp* lidaremulator = context;
    furi_check(lidaremulator);
    
    submenu_reset(lidaremulator->submenu);
}
