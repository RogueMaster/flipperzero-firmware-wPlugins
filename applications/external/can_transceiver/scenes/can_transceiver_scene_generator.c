// Scene code autogenerator.
// Declare scenes to be used in can_transceiver_scene_config.h!

#include "can_transceiver_scene_generator.h"

// Array of scene entry handler pointers.
#define CANTransceiver_ADD_SCENE(name) CANTransceiver_Scene##name##_OnEnter,
void (*const CANTransceiver_Scene_EntryHandlers[])(void* pContext) = {
#include "can_transceiver_scene_config.h"
};
#undef CANTransceiver_ADD_SCENE

// Array of scene event handler pointers.
#define CANTransceiver_ADD_SCENE(name) CANTransceiver_Scene##name##_OnEvent,
bool (*const CANTransceiver_Scene_EventHandlers[])(void* pContext, SceneManagerEvent event) = {
#include "can_transceiver_scene_config.h"
};
#undef CANTransceiver_ADD_SCENE

// Array of scene exit handler pointers.
#define CANTransceiver_ADD_SCENE(name) CANTransceiver_Scene##name##_OnExit,
void (*const CANTransceiver_Scene_ExitHandlers[])(void* pContext) = {
#include "can_transceiver_scene_config.h"
};
#undef CANTransceiver_ADD_SCENE

// Struct of scene handlers.
const SceneManagerHandlers CANTransceiver_Scene_Handlers = {
    .on_enter_handlers = CANTransceiver_Scene_EntryHandlers,
    .on_event_handlers = CANTransceiver_Scene_EventHandlers,
    .on_exit_handlers = CANTransceiver_Scene_ExitHandlers,
    .scene_num = CANTransceiver_SceneNone,
};
