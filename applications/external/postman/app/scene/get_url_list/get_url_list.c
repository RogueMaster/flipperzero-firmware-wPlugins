#include "../../app.h"
#include "../../structs.h"
#include "../../utils/trimwhitespace/trimwhitespace.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

void submenu_callback_select_url(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_select_url");
    furi_assert(context);
    App* app = context;

    // Ensure the index is within bounds
    if(index < app->url_list_count) {
        // Set the selected URL
        strncpy(app->get_state->url, app->url_list[index].url, sizeof(app->get_state->url) - 1);
        app->get_state->url[sizeof(app->get_state->url) - 1] = '\0'; // Ensure null-termination

        // Handle the custom event to move to the URL details scene
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Get_Url_List);
    } else {
        FURI_LOG_E(TAG, "Index out of bounds");
    }
}

void submenu_callback_no_url(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_no_url");
    furi_assert(index);
    App* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Get);
}
void scene_on_enter_get_url_list(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_get_url_list");
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Available URLs");
    submenu_add_item(app->submenu, "Loading ...", 0, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Get_Url_List);

    // Clear the submenu and add the actual URLs
    if(app->url_list && app->url_list_count > 0 && app->url_list[0].url[0] != '\0') {
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "Available URLs");

        // Add URLs to submenu
        for(size_t i = 0; i < app->url_list_count; i++) {
            if(app->url_list[i].url[0] != '\0') {
                char display_name[TEXT_STORE_SIZE];

                // Trim URLs
                trim_whitespace(app->url_list[i].url);

                snprintf(display_name, sizeof(display_name), "%s", app->url_list[i].url);
                submenu_add_item(app->submenu, display_name, i, submenu_callback_select_url, app);
            }
        }
    } else {
        submenu_reset(app->submenu);
        submenu_add_item(app->submenu, "No URLs found", 0, submenu_callback_no_url, app);
    }
}

bool scene_on_event_get_url_list(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_get_url_list");
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_Get_Url_List:
            // Add log before switching to URL details
            FURI_LOG_I(TAG, "Switching to URL details");
            scene_manager_next_scene(app->scene_manager, Get);
            consumed = true;
            break;
        }
        break;

    default: // eg. SceneManagerEventTypeBack, SceneManagerEventTypeTick
        consumed = false;
        break;
    }
    return consumed;
}

void scene_on_exit_get_url_list(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_get_url_list");
    App* app = context;

    submenu_reset(app->submenu);
}
