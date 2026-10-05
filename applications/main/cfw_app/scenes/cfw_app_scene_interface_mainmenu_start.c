#include "../cfw_app.h"

static void mainmenu_start_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_interface_mainmenu_start_on_enter(void* context) {
    CFWApp* app = context;
    submenu_set_header(app->submenu, "Starting App:");
    submenu_add_item(
        app->submenu, "Apps Menu", LoaderMenuIndexApplications, mainmenu_start_callback, app);
    for(size_t i = 0; i < CharList_size(app->mainmenu_app_labels); i++) {
        submenu_add_item(
            app->submenu,
            *CharList_get(app->mainmenu_app_labels, i),
            i,
            mainmenu_start_callback,
            app);
    }
    submenu_add_item(
        app->submenu,
        FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1].name,
        LoaderMenuIndexLast,
        mainmenu_start_callback,
        app);
    submenu_add_item(
        app->submenu, "Settings", LoaderMenuIndexSettings, mainmenu_start_callback, app);
    submenu_set_selected_item(app->submenu, cfw_settings.start_point);
    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewSubmenu);
}

bool cfw_app_scene_interface_mainmenu_start_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event < CharList_size(app->mainmenu_app_labels) ||
       event.event == LoaderMenuIndexApplications || event.event == LoaderMenuIndexLast ||
       event.event == LoaderMenuIndexSettings) {
        cfw_settings.start_point = event.event;
        app->save_settings = true;
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void cfw_app_scene_interface_mainmenu_start_on_exit(void* context) {
    CFWApp* app = context;
    submenu_reset(app->submenu);
}
