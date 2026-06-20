#include "../led_remote.h"

#define NEW_PROFILE_DEFAULT_NAME "New Profile"
#define RENAME_PROFILE_DEFAULT_NAME "Rename Profile"

typedef enum {
    ProfileEditEventConfirm,
} ProfileEditEvent;

static void profile_edit_text_input_callback(void* context) {
    LedRemoteApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ProfileEditEventConfirm);
}

void led_remote_scene_profile_edit_on_enter(void* context) {
    LedRemoteApp* app = context;

    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input,
        app->profile_edit_is_new ? NEW_PROFILE_DEFAULT_NAME : RENAME_PROFILE_DEFAULT_NAME);

    text_input_set_result_callback(
        app->text_input,
        profile_edit_text_input_callback,
        app,
        app->text_input_buf,
        sizeof(app->text_input_buf) - 1,
        false);  // false = don't clear the default text on first press

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdTextInput);
}

bool led_remote_scene_profile_edit_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ProfileEditEventConfirm) {
        // Normalize: underscores → spaces to stay consistent with file-scanned names
        for(size_t i = 0; app->text_input_buf[i]; i++) {
            if(app->text_input_buf[i] == '_') app->text_input_buf[i] = ' ';
        }

        if(strlen(app->text_input_buf) == 0) {
            // Empty name — ignore and go back
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }

        if(app->profile_edit_is_new) {
            // New profile: switch to it (creates the empty file)
            led_remote_switch_profile(app, app->text_input_buf);
        } else {
            led_remote_rename_profile(app, app->profile_selected, app->text_input_buf);
        }

        // Return to profile list (refreshes the scan)
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, LedRemoteSceneProfiles);
    }
    return true;
}

void led_remote_scene_profile_edit_on_exit(void* context) {
    LedRemoteApp* app = context;
    text_input_reset(app->text_input);
}
