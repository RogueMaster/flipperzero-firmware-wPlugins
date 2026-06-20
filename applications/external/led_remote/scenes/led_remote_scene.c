#include "../led_remote.h"

void led_remote_scene_menu_on_enter(void* context);
bool led_remote_scene_menu_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_menu_on_exit(void* context);

void led_remote_scene_remote_on_enter(void* context);
bool led_remote_scene_remote_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_remote_on_exit(void* context);

void led_remote_scene_learn_on_enter(void* context);
bool led_remote_scene_learn_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_learn_on_exit(void* context);

void led_remote_scene_capture_on_enter(void* context);
bool led_remote_scene_capture_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_capture_on_exit(void* context);

void led_remote_scene_profiles_on_enter(void* context);
bool led_remote_scene_profiles_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_profiles_on_exit(void* context);

void led_remote_scene_profile_options_on_enter(void* context);
bool led_remote_scene_profile_options_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_profile_options_on_exit(void* context);

void led_remote_scene_profile_edit_on_enter(void* context);
bool led_remote_scene_profile_edit_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_profile_edit_on_exit(void* context);

void led_remote_scene_style_on_enter(void* context);
bool led_remote_scene_style_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_style_on_exit(void* context);

void led_remote_scene_remote_type_on_enter(void* context);
bool led_remote_scene_remote_type_on_event(void* context, SceneManagerEvent event);
void led_remote_scene_remote_type_on_exit(void* context);

static const AppSceneOnEnterCallback led_remote_on_enter_handlers[] = {
    led_remote_scene_menu_on_enter,
    led_remote_scene_remote_on_enter,
    led_remote_scene_learn_on_enter,
    led_remote_scene_capture_on_enter,
    led_remote_scene_profiles_on_enter,
    led_remote_scene_profile_options_on_enter,
    led_remote_scene_profile_edit_on_enter,
    led_remote_scene_style_on_enter,
    led_remote_scene_remote_type_on_enter,
};

static const AppSceneOnEventCallback led_remote_on_event_handlers[] = {
    led_remote_scene_menu_on_event,
    led_remote_scene_remote_on_event,
    led_remote_scene_learn_on_event,
    led_remote_scene_capture_on_event,
    led_remote_scene_profiles_on_event,
    led_remote_scene_profile_options_on_event,
    led_remote_scene_profile_edit_on_event,
    led_remote_scene_style_on_event,
    led_remote_scene_remote_type_on_event,
};

static const AppSceneOnExitCallback led_remote_on_exit_handlers[] = {
    led_remote_scene_menu_on_exit,
    led_remote_scene_remote_on_exit,
    led_remote_scene_learn_on_exit,
    led_remote_scene_capture_on_exit,
    led_remote_scene_profiles_on_exit,
    led_remote_scene_profile_options_on_exit,
    led_remote_scene_profile_edit_on_exit,
    led_remote_scene_style_on_exit,
    led_remote_scene_remote_type_on_exit,
};

const SceneManagerHandlers led_remote_scene_handlers = {
    .on_enter_handlers = led_remote_on_enter_handlers,
    .on_event_handlers = led_remote_on_event_handlers,
    .on_exit_handlers  = led_remote_on_exit_handlers,
    .scene_num         = LedRemoteSceneNum,
};

bool led_remote_scene_navigation_event_callback(void* context) {
    LedRemoteApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}
