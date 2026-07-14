// Scene that shows a table on how to wire the Flipper to the MCP2515.

// ==== INCLUDES ====

#include "../can_transceiver.h"

// ==== HANDLERS AND CALLBACKS ====

void CANTransceiver_SceneWiring_OnEnter(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;

    widget_add_text_scroll_element(
        pInstance->pWidget,
        0,
        0,
        128,
        64,
        "MCP2515 - Flipper\n"
        "--------------------\n"
        "3V3 - PIN 9\n"
        "GND - PIN 8/11/18\n"
        "SCK - PIN 5\n"
        "CS# - PIN 4\n"
        "MISO - PIN 3\n"
        "MOSI - PIN 2\n"
        "IRQ - PIN 6\n"
        "");

    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewWidget);
}

bool CANTransceiver_SceneWiring_OnEvent(void* pContext, SceneManagerEvent event) {
    UNUSED(pContext);
    UNUSED(event);
    return false;
}

void CANTransceiver_SceneWiring_OnExit(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    widget_reset(pInstance->pWidget);
}
