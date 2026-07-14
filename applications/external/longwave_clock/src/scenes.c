#include "flipper.h"
#include "app_state.h"
#include "scenes.h"
#include "scene_main_menu.h"

/** collection of all scene on_enter handlers */
void (*const lwc_scene_on_enter_handlers[])(void*) = {
    lwc_main_menu_scene_on_enter,
    lwc_sub_menu_scene_on_enter,
    lwc_dcf77_scene_on_enter,
    lwc_msf_scene_on_enter,
    lwc_info_scene_on_enter,
    lwc_about_scene_on_enter};

/** collection of all scene on event handlers */
bool (*const lwc_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    lwc_main_menu_scene_on_event,
    lwc_sub_menu_scene_on_event,
    lwc_dcf77_scene_on_event,
    lwc_msf_scene_on_event,
    lwc_info_scene_on_event,
    lwc_about_scene_on_event};

/** collection of all scene on exit handlers */
void (*const lwc_scene_on_exit_handlers[])(void*) = {
    lwc_main_menu_scene_on_exit,
    lwc_sub_menu_scene_on_exit,
    lwc_dcf77_scene_on_exit,
    lwc_msf_scene_on_exit,
    lwc_info_scene_on_exit,
    lwc_about_scene_on_exit};

/** collection of all on_enter, on_event, on_exit handlers */
const SceneManagerHandlers lwc_scene_manager_handlers = {
    .on_enter_handlers = lwc_scene_on_enter_handlers,
    .on_event_handlers = lwc_scene_on_event_handlers,
    .on_exit_handlers = lwc_scene_on_exit_handlers,
    .scene_num = __lwc_number_of_scenes};

/* callbacks */

/** custom event handler */
bool lwc_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

bool lwc_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void lwc_tick_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}
