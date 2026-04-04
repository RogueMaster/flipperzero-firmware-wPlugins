#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"

enum {
    MainMenuAutoDetect,
    MainMenuHW3,
    MainMenuHW4,
    MainMenuLegacy,
    MainMenuSettings,
    MainMenuAbout,
};

static void main_menu_callback(void* context, uint32_t index) {
    TeslaFSDApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tesla_fsd_scene_main_menu_on_enter(void* context) {
    TeslaFSDApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Tesla FSD Unlock");
    submenu_add_item(app->submenu, "Auto Detect & Start", MainMenuAutoDetect, main_menu_callback, app);
    submenu_add_item(app->submenu, "Force HW3 Mode", MainMenuHW3, main_menu_callback, app);
    submenu_add_item(app->submenu, "Force HW4 Mode", MainMenuHW4, main_menu_callback, app);
    submenu_add_item(app->submenu, "Force Legacy Mode", MainMenuLegacy, main_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", MainMenuSettings, main_menu_callback, app);
    submenu_add_item(app->submenu, "About", MainMenuAbout, main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewSubmenu);
}

bool tesla_fsd_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    TeslaFSDApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MainMenuAutoDetect:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_hw_detect);
            consumed = true;
            break;
        case MainMenuHW3:
            app->hw_version = TeslaHW_HW3;
            fsd_state_init(&app->fsd_state, TeslaHW_HW3);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case MainMenuHW4:
            app->hw_version = TeslaHW_HW4;
            fsd_state_init(&app->fsd_state, TeslaHW_HW4);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case MainMenuLegacy:
            app->hw_version = TeslaHW_Legacy;
            fsd_state_init(&app->fsd_state, TeslaHW_Legacy);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case MainMenuSettings:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_settings);
            consumed = true;
            break;
        case MainMenuAbout:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_about);
            consumed = true;
            break;
        }
    }
    return consumed;
}

void tesla_fsd_scene_main_menu_on_exit(void* context) {
    TeslaFSDApp* app = context;
    submenu_reset(app->submenu);
}
