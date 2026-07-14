#include "mizip_balance_editor_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const mizip_balance_editor_on_enter_handlers[])(void*) = {
#include "mizip_balance_editor_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const mizip_balance_editor_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "mizip_balance_editor_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const mizip_balance_editor_on_exit_handlers[])(void* context) = {
#include "mizip_balance_editor_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers mizip_balance_editor_scene_handlers = {
    .on_enter_handlers = mizip_balance_editor_on_enter_handlers,
    .on_event_handlers = mizip_balance_editor_on_event_handlers,
    .on_exit_handlers = mizip_balance_editor_on_exit_handlers,
    .scene_num = MiZipBalanceEditorSceneNum,
};
