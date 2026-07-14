
#include "../app.h"
#include "../structs.h" // Has all the Id's / Enums
#include "./view_tasks/view_tasks.h"
#include <gui/modules/dialog_ex.h>
#include <gui/view.h>
// scenes
#include "./edit_description/edit_description.h"
#include "./edit_name/edit_name.h"
#include "./edit_price/edit_price.h"
#include "./edit_task/edit_task.h"
#include "./main_menu/main_menu.h"
#include "./task_continue/task_continue.h"
#include "./view_stats/view_stats.h"
#include "./view_task/view_task.h"

#include <time_tracker_icons.h> // created later after ubft run

/** collection of all scene on_enter handlers - in the same order as their enum
 */
void (*const scene_on_enter_handlers[])(void*) = {
    scene_on_enter_main_menu,
    scene_on_enter_view_tasks,
    scene_on_enter_task_actions,
    scene_on_enter_task_continue,
    scene_on_enter_view_stats,
    scene_on_enter_edit_task,
    scene_on_enter_task_name_input,
    scene_on_enter_task_description_input,
    scene_on_enter_price_input};

/** collection of all scene on event handlers - in the same order as their enum
 */
bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    scene_on_event_main_menu,
    scene_on_event_view_tasks,
    scene_on_event_task_actions,
    scene_on_event_task_continue,
    scene_on_event_view_stats,
    scene_on_event_edit_task,
    scene_on_event_task_name_input,
    scene_on_event_task_description_input,
    scene_on_event_price_input};

/** collection of all scene on exit handlers - in the same order as their enum
 */
void (*const scene_on_exit_handlers[])(void*) = {
    scene_on_exit_main_menu,
    scene_on_exit_view_tasks,
    scene_on_exit_task_actions,
    scene_on_exit_task_continue,
    scene_on_exit_view_stats,
    scene_on_exit_edit_task,
    scene_on_exit_task_name_input,
    scene_on_exit_task_description_input,
    scene_on_exit_price_input};

/** collection of all on_enter, on_event, on_exit handlers */
const SceneManagerHandlers scene_event_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = Count};

/** custom event handler - passes the event to the scene manager */
bool scene_manager_custom_event_callback(void* context, uint32_t custom_event) {
    FURI_LOG_T(TAG, "scene_manager_custom_event_callback");
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

/** navigation event handler - passes the event to the scene manager */
bool scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(TAG, "scene_manager_navigation_event_callback");
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/** initialise the scene manager with all handlers */
void scene_manager_init(App* app) {
    FURI_LOG_T(TAG, "scene_manager_init");
    app->scene_manager = scene_manager_alloc(&scene_event_handlers, app);
}

/** initialise the views, and initialise the view dispatcher with all views */
void view_dispatcher_init(App* app) {
    FURI_LOG_T(TAG, "view_dispatcher_init");
    app->view_dispatcher = view_dispatcher_alloc();

    // allocate each view
    FURI_LOG_D(TAG, "view_dispatcher_init allocating views");
    app->menu = menu_alloc();
    app->submenu = submenu_alloc();
    app->number_input = number_input_alloc();
    app->text_input = text_input_alloc();
    app->variable_item_list = variable_item_list_alloc();
    app->submenu_task_actions = submenu_alloc();
    app->dialog = dialog_ex_alloc();
    app->view = view_alloc();
    app->text_store[0] = '\0';
    // Initialize and start the timer to update every second
    app->timer = furi_timer_alloc(task_continue_timer_callback, FuriTimerTypePeriodic, app);

    // assign callback that pass events from views to the scene manager
    FURI_LOG_D(TAG, "view_dispatcher_init setting callbacks");
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, scene_manager_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, scene_manager_navigation_event_callback);

    // MAIN MENU
    // add views to the dispatcher, indexed by their enum value
    FURI_LOG_D(TAG, "view_dispatcher_init adding view menu");
    view_dispatcher_add_view(app->view_dispatcher, AppView_Menu, menu_get_view(app->menu));

    // VIEW TASKS
    FURI_LOG_D(TAG, "view_dispatcher_init adding view submenu");
    view_dispatcher_add_view(
        app->view_dispatcher, AppView_ViewTasks, submenu_get_view(app->submenu));

    // TASK ACTIONS
    FURI_LOG_D(TAG, "view_dispatcher_init adding view task actions");
    view_dispatcher_add_view(
        app->view_dispatcher, AppView_TaskActions, submenu_get_view(app->submenu_task_actions));

    // TASK CONTINUE
    FURI_LOG_D(TAG, "view_dispatcher_init adding view task continue");
    view_dispatcher_add_view(
        app->view_dispatcher, AppView_TaskContinue, dialog_ex_get_view(app->dialog));

    // EDIT MENU
    FURI_LOG_D(TAG, "view_dispatcher_init adding view edit menu");
    view_dispatcher_add_view(app->view_dispatcher, AppView_ViewStats, app->view);

    view_dispatcher_add_view(
        app->view_dispatcher,
        AppView_EditTask,
        variable_item_list_get_view(app->variable_item_list));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_TaskNameInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, AppView_TaskDescriptionInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, AppView_NumberInput, number_input_get_view(app->number_input));
}
