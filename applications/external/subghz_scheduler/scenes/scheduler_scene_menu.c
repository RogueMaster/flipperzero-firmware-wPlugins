#include "src/scheduler_app_i.h"

#include <gui/modules/submenu.h>

#define TAG "SubGHzSchedulerMenu"

typedef enum {
    SchedulerMenuItemNewSchedule = 0,
    SchedulerMenuItemLoadSchedule,
    SchedulerMenuItemAbout,
} SchedulerMenuItem;

static void scheduler_scene_menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void scheduler_scene_menu_on_enter(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;

    submenu_reset(app->menu);
    submenu_set_header(app->menu, SCHEDULER_APP_NAME);

    submenu_add_item(
        app->menu,
        "New Schedule",
        SchedulerMenuItemNewSchedule,
        scheduler_scene_menu_callback,
        app);
    submenu_add_item(
        app->menu,
        "Load Schedule",
        SchedulerMenuItemLoadSchedule,
        scheduler_scene_menu_callback,
        app);
    submenu_add_item(
        app->menu, "About", SchedulerMenuItemAbout, scheduler_scene_menu_callback, app);

    submenu_set_selected_item(
        app->menu, scene_manager_get_scene_state(app->scene_manager, SchedulerSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewMenu);
}

bool scheduler_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SchedulerApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, SchedulerSceneMenu, event.event);

        switch(event.event) {
        case SchedulerMenuItemNewSchedule:
            app->should_reset = true;
            scene_manager_next_scene(app->scene_manager, SchedulerSceneStart);
            return true;
        case SchedulerMenuItemLoadSchedule:
            app->should_reset = false;
            scene_manager_next_scene(app->scene_manager, SchedulerSceneLoadSchedule);
            return true;
        case SchedulerMenuItemAbout:
            app->should_reset = false;
            scene_manager_next_scene(app->scene_manager, SchedulerSceneAbout);
            return true;
        default:
            return true;
        }
    }

    return false;
}

void scheduler_scene_menu_on_exit(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    submenu_reset(app->menu);
}
