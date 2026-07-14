#include <gui/scene_manager.h>

// Create the enumerator for the scenes
#define ADD_SCENE(prefix, name, id) app_scene_##id,
typedef enum {
#include "app_scene_config.h"
    app_scene_enum,
} Appscenes;
#undef ADD_SCENE

// Define the app_scene_handlers as a extern variable
extern const SceneManagerHandlers app_scene_handlers;

// Define the functions for the on enter scenes
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "app_scene_config.h"
#undef ADD_SCENE

// Define the functions for the on event scenes
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "app_scene_config.h"
#undef ADD_SCENE

// Define the functions for the on exit scenes
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "app_scene_config.h"
#undef ADD_SCENE
