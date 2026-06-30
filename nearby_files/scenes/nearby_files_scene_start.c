#include "../nearby_files.h"
#include "nearby_files_scene.h"

void nearby_files_scene_start_on_enter(void* context) {
    NearbyFilesApp* app = context;
    
    // Reset widget and switch to view - GPS timer will handle display updates
    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, NearbyFilesViewWidget);
    
    // Start GPS waiting process
    nearby_files_start_gps_wait(app);
}

bool nearby_files_scene_start_on_event(void* context, SceneManagerEvent event) {
    NearbyFilesApp* app = context;
    bool consumed = false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case NearbyFilesCustomEventStartScan:
                // Update widget to show scanning message
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Calculating distances...");
                
                // Scan directories for files
                if(nearby_files_scan_directories(app)) {
                    if(app->file_count > 0) {
                        scene_manager_next_scene(app->scene_manager, NearbyFilesSceneFileList);
                    } else {
                        widget_reset(app->widget);
                        widget_add_string_element(
                            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No files found");
                        widget_add_string_element(
                            app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Press Back to exit");
                    }
                } else {
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Scan failed");
                    widget_add_string_element(
                        app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Press Back to exit");
                }
                consumed = true;
                break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Go to menu when back button is pressed during GPS waiting or scanning
        scene_manager_next_scene(app->scene_manager, NearbyFilesSceneMenu);
        consumed = true;
    }
    
    return consumed;
}

void nearby_files_scene_start_on_exit(void* context) {
    NearbyFilesApp* app = context;
    widget_reset(app->widget);
}
