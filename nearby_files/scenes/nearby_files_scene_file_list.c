#include "../nearby_files.h"
#include "nearby_files_scene.h"

void nearby_files_scene_file_list_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    // Populate list with found files
    nearby_files_populate_list(app);
    
    // Switch to variable item list view
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewVariableItemList);
}

bool nearby_files_scene_file_list_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NearbyFilesCustomEventFileSelected) {
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Go to menu when back button is pressed in file list
        scene_manager_next_scene(app->scene_manager, NearbyFilesSceneMenu);
        consumed = true;
    }
    
    return consumed;
}

void nearby_files_scene_file_list_on_exit(void* context) {
    NearbyFilesApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
