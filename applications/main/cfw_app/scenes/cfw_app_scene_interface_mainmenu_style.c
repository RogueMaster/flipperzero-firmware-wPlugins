#include "../cfw_app.h"
#include <loader/loader.h>

// Reference the menu style names from mainmenu scene
extern const char* const menu_style_names[MenuStyleCount];

void cfw_app_scene_interface_mainmenu_style_submenu_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_interface_mainmenu_style_on_enter(void* context) {
    CFWApp* app = context;
    Submenu* submenu = app->submenu;

    for(size_t i = 0; i < MenuStyleCount; i++) {
        submenu_add_item(
            submenu,
            menu_style_names[i],
            i,
            cfw_app_scene_interface_mainmenu_style_submenu_callback,
            app);
    }

    submenu_set_header(submenu, "Choose Menu Style:");
    submenu_set_selected_item(
        submenu,
        (uint32_t)cfw_settings.menu_style < MenuStyleCount ? cfw_settings.menu_style :
                                                             MenuStyleList);
    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewSubmenu);
}

bool cfw_app_scene_interface_mainmenu_style_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event >= MenuStyleCount) return consumed;
        cfw_settings.menu_style = event.event;
        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_set_menu_style(loader, cfw_menu_style_get_plugin_name(cfw_settings.menu_style));
        furi_record_close(RECORD_LOADER);
        app->save_settings = true;
        scene_manager_previous_scene(app->scene_manager);
    }

    return consumed;
}

void cfw_app_scene_interface_mainmenu_style_on_exit(void* context) {
    CFWApp* app = context;
    submenu_reset(app->submenu);
}
