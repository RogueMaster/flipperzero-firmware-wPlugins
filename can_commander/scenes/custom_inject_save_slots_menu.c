#include "../can_commander.h"

#include <stdio.h>

typedef enum {
    CustomInjectSaveSlotsSetName = 0,
    CustomInjectSaveSlotsSave,
} CustomInjectSaveSlotsMenuIndex;

static char cancommander_custom_inject_save_name_item[48];

static void cancommander_scene_custom_inject_save_slots_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_custom_inject_save_slots_menu_on_enter(void* context) {
    App* app = context;

    if(app->custom_inject_set_name[0] == '\0') {
        snprintf(
            app->custom_inject_set_name,
            sizeof(app->custom_inject_set_name),
            "inj_profile");
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Save Injection Profile");

    snprintf(
        cancommander_custom_inject_save_name_item,
        sizeof(cancommander_custom_inject_save_name_item),
        "Profile Name: %s",
        app->custom_inject_set_name);

    submenu_add_item(
        app->submenu,
        cancommander_custom_inject_save_name_item,
        CustomInjectSaveSlotsSetName,
        cancommander_scene_custom_inject_save_slots_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save to SD",
        CustomInjectSaveSlotsSave,
        cancommander_scene_custom_inject_save_slots_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(
            app->scene_manager, cancommander_scene_custom_inject_save_slots_menu));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_custom_inject_save_slots_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_custom_inject_save_slots_menu, event.event);

    switch(event.event) {
    case CustomInjectSaveSlotsSetName:
        app_begin_edit(
            app,
            app->custom_inject_set_name,
            sizeof(app->custom_inject_set_name),
            "Set Name");
        scene_manager_next_scene(app->scene_manager, cancommander_scene_text_input);
        return true;

    case CustomInjectSaveSlotsSave:
        app_custom_inject_save_slot_set(app, app->custom_inject_set_name);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_custom_inject_save_slots_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
