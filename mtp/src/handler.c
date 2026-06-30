#include "main.h"

bool scene_handler_event_forwarder(void* context, uint32_t event_id) {
    App* app = (App*)context;
    if(app == NULL || app->scene_manager == NULL) {
        return false;
    }

    return scene_manager_handle_custom_event(app->scene_manager, event_id);
}

bool scene_handler_navigation_forwarder(void* context) {
    App* app = (App*)context;
    if(app == NULL || app->scene_manager == NULL) {
        return false;
    }

    return scene_manager_handle_back_event(app->scene_manager);
}
