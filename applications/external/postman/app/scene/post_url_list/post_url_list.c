#include "../../app.h"
#include "../../utils/trimwhitespace/trimwhitespace.h"

void submenu_callback_select_post_url(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_select_url");
    furi_assert(context);
    App* app = context;

    // Ensure the index is within bounds
    if(index < app->post_url_list_count) {
        // Set the selected URL
        strncpy(
            app->post_state->url, app->post_url_list[index].url, sizeof(app->post_state->url) - 1);
        app->post_state->url[sizeof(app->post_state->url) - 1] = '\0'; // Ensure null-termination
        app->post_state->payload =
            furi_string_alloc_set(furi_string_get_cstr(app->post_url_list[index].payload));

        // Handle the custom event to move to the URL details scene
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Post_Url_List);
    } else {
        FURI_LOG_E(TAG, "Index out of bounds");
    }
}

void submenu_callback_no_post_url(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_no_url");
    furi_assert(index);
    App* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Post);
}

void scene_on_enter_post_url_list(void* context) {
    FURI_LOG_T(TAG, "Displaying Post Url view");
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Available URLs");
    submenu_add_item(app->submenu, "Loading ...", 0, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Post_Url_List);

    // Clear the submenu and add the actual URLs
    if(app->post_url_list && app->post_url_list_count > 0 &&
       app->post_url_list[0].url[0] != '\0') {
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "Available URLs");

        // Add URLs to submenu
        for(size_t i = 0; i < app->post_url_list_count; i++) {
            if(app->post_url_list[i].url[0] != '\0') {
                char display_name[TEXT_STORE_SIZE];

                // Trim URLs
                trim_whitespace(app->post_url_list[i].url);

                snprintf(display_name, sizeof(display_name), "%s", app->post_url_list[i].url);
                submenu_add_item(
                    app->submenu, display_name, i, submenu_callback_select_post_url, app);
            }
        }
    } else {
        submenu_reset(app->submenu);
        submenu_add_item(app->submenu, "No URLs found", 0, submenu_callback_no_post_url, app);
    }
}

void scene_on_exit_post_url_list(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

bool scene_on_event_post_url_list(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_post_url_list");
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_Post_Url_List:
            // Add log before switching to URL details
            FURI_LOG_I(TAG, "Switching to URL details");
            scene_manager_next_scene(app->scene_manager, Post);
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
