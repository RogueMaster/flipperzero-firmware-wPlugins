#include "../led_remote.h"

// Special index reserved for "New Profile"
#define NEW_PROFILE_INDEX 0xFF

static void led_remote_scene_profiles_callback(void* context, uint32_t index) {
    LedRemoteApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void led_remote_scene_profiles_on_enter(void* context) {
    LedRemoteApp* app = context;

    led_remote_scan_profiles(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Profiles");

    uint32_t current_idx = 0;
    for(uint8_t i = 0; i < app->profile_count; i++) {
        bool active = (strncmp(app->profiles[i], app->profile_name, sizeof(app->profiles[0]) - 1) == 0);
        FuriString* label = furi_string_alloc_printf(
            "%s%s", active ? "> " : "  ", app->profiles[i]);
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(label),
            i,
            led_remote_scene_profiles_callback,
            app);
        furi_string_free(label);
        if(active) current_idx = i;
    }

    submenu_add_item(
        app->submenu,
        "[+ New Profile]",
        NEW_PROFILE_INDEX,
        led_remote_scene_profiles_callback,
        app);

    submenu_set_selected_item(app->submenu, current_idx);
    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
}

bool led_remote_scene_profiles_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == NEW_PROFILE_INDEX) {
        // New profile: empty text → ProfileEdit
        app->profile_edit_is_new = true;
        app->text_input_buf[0] = '\0';
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneProfileEdit);
    } else if(event.event < app->profile_count) {
        // Existing profile → options submenu
        strncpy(app->profile_selected, app->profiles[event.event], sizeof(app->profile_selected) - 1);
        app->profile_selected[sizeof(app->profile_selected) - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneProfileOptions);
    }
    return true;
}

void led_remote_scene_profiles_on_exit(void* context) {
    LedRemoteApp* app = context;
    submenu_reset(app->submenu);
}
