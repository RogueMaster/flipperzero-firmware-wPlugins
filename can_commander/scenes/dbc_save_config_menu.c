#include "../can_commander.h"

#include <stdio.h>

typedef enum {
    DbcSaveConfigSetName = 0,
    DbcSaveConfigSave,
} DbcSaveConfigMenuIndex;

static char cancommander_dbc_save_name_item[48];

static void cancommander_scene_dbc_save_config_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_dbc_save_config_menu_on_enter(void* context) {
    App* app = context;

    if(app->dbc_config_save_name[0] == '\0') {
        snprintf(app->dbc_config_save_name, sizeof(app->dbc_config_save_name), "dbc_profile");
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Save DBC Profile");

    snprintf(
        cancommander_dbc_save_name_item,
        sizeof(cancommander_dbc_save_name_item),
        "Profile Name: %s",
        app->dbc_config_save_name);

    submenu_add_item(
        app->submenu,
        cancommander_dbc_save_name_item,
        DbcSaveConfigSetName,
        cancommander_scene_dbc_save_config_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save to SD",
        DbcSaveConfigSave,
        cancommander_scene_dbc_save_config_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_dbc_save_config_menu));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_dbc_save_config_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_dbc_save_config_menu, event.event);

    switch(event.event) {
    case DbcSaveConfigSetName:
        app_begin_edit(
            app,
            app->dbc_config_save_name,
            sizeof(app->dbc_config_save_name),
            "Config Name");
        scene_manager_next_scene(app->scene_manager, cancommander_scene_text_input);
        return true;

    case DbcSaveConfigSave:
        app_dbc_config_save_file(app, app->dbc_config_save_name);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_dbc_save_config_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
