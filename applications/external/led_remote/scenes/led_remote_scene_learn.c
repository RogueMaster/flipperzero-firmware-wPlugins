#include "../led_remote.h"

static void led_remote_scene_learn_callback(void* context, uint32_t index) {
    LedRemoteApp* app = context;
    app->learn_target = (LedButton)index;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void led_remote_scene_learn_on_enter(void* context) {
    LedRemoteApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Learn Buttons");

    for(size_t i = 0; i < app->button_count; i++) {
        FuriString* label = furi_string_alloc_printf(
            "%s %s", app->learned[i] ? "[OK]" : "[  ]", app->button_names[i]);
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(label),
            (uint32_t)i,
            led_remote_scene_learn_callback,
            app);
        furi_string_free(label);
    }

    submenu_set_selected_item(app->submenu, app->learn_target);
    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
}

bool led_remote_scene_learn_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_next_scene(app->scene_manager, LedRemoteSceneCapture);
    return true;
}

void led_remote_scene_learn_on_exit(void* context) {
    LedRemoteApp* app = context;
    submenu_reset(app->submenu);
}
