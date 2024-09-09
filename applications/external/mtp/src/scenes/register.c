#include "../main.h"

/**
 * SceneManagerHandlers initialization using the macro.
 */
void (*const scene_on_enter_handlers[])(void* context) = {
#define SCENE_ACTION(scene) scene##_on_enter,
#include "list.h"
#undef SCENE_ACTION
};

bool (*const scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#define SCENE_ACTION(scene) scene##_on_event,
#include "list.h"
#undef SCENE_ACTION
};

void (*const scene_on_exit_handlers[])(void* context) = {
#define SCENE_ACTION(scene) scene##_on_exit,
#include "list.h"
#undef SCENE_ACTION
};

const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = AppSceneNum,
};

/**
 * Register all scenes.
 */

void register_scenes(App* app) {
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);
    furi_assert(app->scene_manager != NULL, "Failed to allocate scene manager.");

    app->view_dispatcher = view_dispatcher_alloc();
    furi_assert(app->view_dispatcher != NULL, "Failed to allocate view dispatcher.");

    if(app->allocated_scenes == NULL) {
        app->allocated_scenes = (void**)malloc(sizeof(void*) * AppSceneNum);
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, scene_handler_event_forwarder);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, scene_handler_navigation_forwarder);

    View* view = NULL;
#define SCENE_ACTION(scene)                                                                   \
    app->allocated_scenes[scene] = (void*)scene##_alloc();                                    \
    furi_assert(                                                                              \
        app->allocated_scenes[scene] != NULL, "Failed to allocate scene: " STRINGIFY(scene)); \
    view = scene##_get_view(app->allocated_scenes[scene]);                                    \
    furi_assert(view != NULL, "Failed to get view for scene: " STRINGIFY(scene));             \
    view_dispatcher_add_view(app->view_dispatcher, scene, view);
#include "list.h"
#undef SCENE_ACTION
}

/**
 * Free all scenes.
 */
void free_scenes(App* app) {
    FURI_LOG_I("DemoApp", "Freeing scenes.");
    furi_assert(app != NULL, "App is NULL.");
    void* tmp;
#define SCENE_ACTION(scene)                                                                    \
    if(app->allocated_scenes != NULL) {                                                        \
        tmp = app->allocated_scenes[scene];                                                    \
        app->allocated_scenes[scene] = NULL;                                                   \
        FURI_LOG_I("DemoApp", "Freeing scene " STRINGIFY(scene) ".");                          \
        if(tmp != NULL) scene##_free(tmp);                                                     \
        FURI_LOG_I("DemoApp", "Free'd scene " STRINGIFY(scene) ".");                           \
    }                                                                                          \
    if(app->view_dispatcher != NULL) view_dispatcher_remove_view(app->view_dispatcher, scene); \
    FURI_LOG_I("DemoApp", "Removed from dispatcher " STRINGIFY(scene) ".");

#include "list.h"
#undef SCENE_ACTION

    FURI_LOG_I("DemoApp", "Freeing allocated scenes.");
    furi_assert(app->allocated_scenes != NULL, "Allocated scenes is NULL.");
    free(app->allocated_scenes);
    app->allocated_scenes = NULL;

    FURI_LOG_I("DemoApp", "Freeing View dispatcher.");
    furi_assert(app->view_dispatcher != NULL, "View dispatcher is NULL.");
    view_dispatcher_free(app->view_dispatcher);

    FURI_LOG_I("DemoApp", "Freeing SceneManager");
    furi_assert(app->scene_manager != NULL, "Scene manager is NULL.");
    scene_manager_free(app->scene_manager);

    FURI_LOG_I("DemoApp", "Freeing App");
    free(app);
}
