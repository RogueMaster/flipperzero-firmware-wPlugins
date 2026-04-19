#include "../disney_toolbox_app.h"

#define MB_MENU_EVENT_PRESETS (MagicBandCodeTypeCount)
#define MB_MENU_EVENT_ABOUT (MagicBandCodeTypeCount + 1)

static void disney_toolbox_app_scene_mb_menu_submenu_callback(void* context, uint32_t index) {
    DisneyToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disney_toolbox_app_scene_mb_menu_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "MagicBand+ Beacon");

    for (uint32_t i = 0; i < MagicBandCodeTypeCount; i++) {
        submenu_add_item(submenu, magicband_code_info[i].name, i,
                         disney_toolbox_app_scene_mb_menu_submenu_callback, app);
    }

    submenu_add_item(submenu, "Presets", MB_MENU_EVENT_PRESETS,
                     disney_toolbox_app_scene_mb_menu_submenu_callback, app);
    submenu_add_item(submenu, "About", MB_MENU_EVENT_ABOUT,
                     disney_toolbox_app_scene_mb_menu_submenu_callback, app);

    uint32_t selected =
        scene_manager_get_scene_state(app->scene_manager, DisneyToolboxAppSceneMbMenu);
    submenu_set_selected_item(submenu, selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewSubmenu);
}

bool disney_toolbox_app_scene_mb_menu_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == MB_MENU_EVENT_PRESETS) {
            scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMbMenu,
                                          event.event);
            scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMbPresets, 0);
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneMbPresets);
            consumed = true;
        } else if (event.event == MB_MENU_EVENT_ABOUT) {
            scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMbMenu,
                                          event.event);
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneMbAbout);
            consumed = true;
        } else if (event.event < MagicBandCodeTypeCount) {
            scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMbMenu,
                                          event.event);
            scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMbConfig, 0);
            app->selected_code_type = (MagicBandCodeType)event.event;
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneMbConfig);
            consumed = true;
        }
    }

    return consumed;
}

void disney_toolbox_app_scene_mb_menu_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    submenu_reset(app->submenu);
}
