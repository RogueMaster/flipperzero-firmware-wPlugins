#include "nearby_files_scene.h"
#include "../nearby_files.h"

void nearby_files_scene_about_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewAbout);
}

bool nearby_files_scene_about_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;
    
    if(event.type == SceneManagerEventTypeBack) {
        // Go back to menu when back button is pressed
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    
    return consumed;
}

void nearby_files_scene_about_on_exit(void* context) {
    UNUSED(context);
}
