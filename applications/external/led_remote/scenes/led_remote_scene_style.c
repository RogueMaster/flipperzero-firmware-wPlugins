#include "../led_remote.h"

static const char* const STYLE_NAMES[LedRemoteLayoutCount] = {
    "Circle",
    "Grid",
};

static void led_remote_scene_style_callback(void* context, uint32_t index) {
    LedRemoteApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void led_remote_scene_style_on_enter(void* context) {
    LedRemoteApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Remote Style");

    for(size_t i = 0; i < LedRemoteLayoutCount; i++) {
        FuriString* label = furi_string_alloc_printf(
            "%s%s",
            (app->layout == (LedRemoteLayout)i) ? "> " : "  ",
            STYLE_NAMES[i]);
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(label),
            i,
            led_remote_scene_style_callback,
            app);
        furi_string_free(label);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
}

bool led_remote_scene_style_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event < (uint32_t)LedRemoteLayoutCount) {
        app->layout = (LedRemoteLayout)event.event;
    }
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void led_remote_scene_style_on_exit(void* context) {
    LedRemoteApp* app = context;
    submenu_reset(app->submenu);
}
