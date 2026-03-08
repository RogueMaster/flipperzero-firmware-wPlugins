#include "../can_commander.h"

typedef enum {
    BusFilterCan0 = 0,
    BusFilterCan1,
    BusFilterClearCan0,
    BusFilterClearCan1,
} BusFilterMenuIndex;

static void cancommander_scene_bus_filter_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_bus_filter_apply_can0(App* app) {
    app_action_bus_set_filter(app, CcBusCan0, app->args_filter_can0);
}

static void cancommander_scene_bus_filter_apply_can1(App* app) {
    app_action_bus_set_filter(app, CcBusCan1, app->args_filter_can1);
}

void cancommander_scene_bus_filter_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "CAN0 Filter",
        BusFilterCan0,
        cancommander_scene_bus_filter_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "CAN1 Filter",
        BusFilterCan1,
        cancommander_scene_bus_filter_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Clear CAN0 Filter",
        BusFilterClearCan0,
        cancommander_scene_bus_filter_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Clear CAN1 Filter",
        BusFilterClearCan1,
        cancommander_scene_bus_filter_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, cancommander_scene_bus_filter_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_bus_filter_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_bus_filter_menu, event.event);

    switch(event.event) {
    case BusFilterCan0:
        app_begin_args_editor_apply(
            app,
            app->args_filter_can0,
            sizeof(app->args_filter_can0),
            "CAN0 Filter",
            "Set CAN0",
            cancommander_scene_bus_filter_apply_can0,
            cancommander_scene_status);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case BusFilterCan1:
        app_begin_args_editor_apply(
            app,
            app->args_filter_can1,
            sizeof(app->args_filter_can1),
            "CAN1 Filter",
            "Set CAN1",
            cancommander_scene_bus_filter_apply_can1,
            cancommander_scene_status);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case BusFilterClearCan0:
        app_action_bus_clear_filter(app, CcBusCan0);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case BusFilterClearCan1:
        app_action_bus_clear_filter(app, CcBusCan1);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_bus_filter_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
