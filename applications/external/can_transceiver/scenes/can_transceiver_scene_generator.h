// Header for can_transceiver_scene_generator.h.

#ifndef CAN_TRANSCEIVER_SCENE_H
#define CAN_TRANSCEIVER_SCENE_H

// ==== INCLUDES ====

#include <gui/scene_manager.h>

// ==== TYPEDEFS ====

// Generates an enum of scenes.
#define CANTransceiver_ADD_SCENE(name) CANTransceiver_Scene##name,
typedef enum {
#include "can_transceiver_scene_config.h"
    CANTransceiver_SceneNone,
} CANTransceiver_Scene;
#undef CANTransceiver_ADD_SCENE

// ==== EXTERNS ====

extern const SceneManagerHandlers CANTransceiver_Scene_Handlers;

// ==== HANDLER DECLARATIONS ====

// Scene entry handler prototypes.
#define CANTransceiver_ADD_SCENE(name) void CANTransceiver_Scene##name##_OnEnter(void* pContext);
#include "can_transceiver_scene_config.h"
#undef CANTransceiver_ADD_SCENE

// Scene event handler prototypes.
#define CANTransceiver_ADD_SCENE(name) \
    bool CANTransceiver_Scene##name##_OnEvent(void* pContext, SceneManagerEvent event);
#include "can_transceiver_scene_config.h"
#undef CANTransceiver_ADD_SCENE

// Scene exit handler prototypes.
#define CANTransceiver_ADD_SCENE(name) void CANTransceiver_Scene##name##_OnExit(void* pContext);
#include "can_transceiver_scene_config.h"
#undef CANTransceiver_ADD_SCENE

#endif // CAN_TRANSCEIVER_SCENE_H
