#include "../glitch_trigger_i.h"
#include <string.h>

/* Profiles list: "[+] Save current..." followed by every saved profile. Saving
 * jumps to the name-entry scene; picking a profile opens the load/delete
 * action scene. */

#define PROFILE_INDEX_SAVE 0
#define PROFILE_INDEX_FIRST 1

static char s_names[GLITCH_PROFILE_MAX][GLITCH_PROFILE_NAME_MAX];
static size_t s_count;

static void glitch_scene_profiles_cb(void* context, uint32_t index) {
    GlitchApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void glitch_scene_profiles_on_enter(void* context) {
    GlitchApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Profiles");
    submenu_add_item(
        submenu, "[+] Save current...", PROFILE_INDEX_SAVE, glitch_scene_profiles_cb, app);

    s_count = glitch_storage_list_profiles(s_names, GLITCH_PROFILE_MAX);
    for(size_t i = 0; i < s_count; i++) {
        submenu_add_item(
            submenu, s_names[i], PROFILE_INDEX_FIRST + i, glitch_scene_profiles_cb, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GlitchSceneProfiles));
    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewSubmenu);
}

bool glitch_scene_profiles_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, GlitchSceneProfiles, event.event);
        if(event.event == PROFILE_INDEX_SAVE) {
            app->name_buf[0] = '\0';
            scene_manager_next_scene(app->scene_manager, GlitchSceneProfileName);
            consumed = true;
        } else {
            size_t i = event.event - PROFILE_INDEX_FIRST;
            if(i < s_count) {
                strncpy(app->selected_profile, s_names[i], GLITCH_PROFILE_NAME_MAX - 1);
                app->selected_profile[GLITCH_PROFILE_NAME_MAX - 1] = '\0';
                scene_manager_next_scene(app->scene_manager, GlitchSceneProfileAct);
                consumed = true;
            }
        }
    }
    return consumed;
}

void glitch_scene_profiles_on_exit(void* context) {
    GlitchApp* app = context;
    submenu_reset(app->submenu);
}
