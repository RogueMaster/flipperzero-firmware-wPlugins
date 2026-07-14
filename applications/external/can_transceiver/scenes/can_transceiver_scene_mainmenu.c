// Main menu scene.

// ==== INCLUDES ====

// App-specific
#include "../can_transceiver.h"

// ==== TYPEDEFS ====

typedef enum {
    CANTransceiver_SceneMainMenu_Receive,
    CANTransceiver_SceneMainMenu_Saved,
    CANTransceiver_SceneMainMenu_Transmit,
    CANTransceiver_SceneMainMenu_OBDIIMode,
    CANTransceiver_SceneMainMenu_Setup,
    CANTransceiver_SceneMainMenu_Wiring,
    CANTransceiver_SceneMainMenu_About,
} CANTransceiver_SceneMainMenu_Index;

// ==== HANDLERS AND CALLBACKS ====

static void CANTransceiver_SceneMainMenu_SubmenuCallback(void* pContext, uint32_t u32Index) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    view_dispatcher_send_custom_event(pInstance->pViewDispatcher, u32Index);
}

void CANTransceiver_SceneMainMenu_OnEnter(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;

    // Create the submenu now that we're in it.
    submenu_add_item(
        pInstance->pSubmenu,
        "Receive",
        CANTransceiver_SceneMainMenu_Receive,
        CANTransceiver_SceneMainMenu_SubmenuCallback,
        pInstance);
    submenu_add_item(
        pInstance->pSubmenu,
        "Transmit",
        CANTransceiver_SceneMainMenu_Transmit,
        CANTransceiver_SceneMainMenu_SubmenuCallback,
        pInstance);
    // TODO: I won't bother with OBD-II mode for now. First let's get the base features done, and add this if I want it.
    // Most likely I do want it, a code reader will be super useful if I get a check engine light again.
#if 0
	submenu_add_item(pInstance->pSubmenu, "OBD-II Mode", CANTransceiver_SceneMainMenu_OBDIIMode,
		CANTransceiver_SceneMainMenu_SubmenuCallback, pInstance);
#endif
    submenu_add_item(
        pInstance->pSubmenu,
        "Setup",
        CANTransceiver_SceneMainMenu_Setup,
        CANTransceiver_SceneMainMenu_SubmenuCallback,
        pInstance);
    submenu_add_item(
        pInstance->pSubmenu,
        "Wiring",
        CANTransceiver_SceneMainMenu_Wiring,
        CANTransceiver_SceneMainMenu_SubmenuCallback,
        pInstance);
    submenu_add_item(
        pInstance->pSubmenu,
        "About",
        CANTransceiver_SceneMainMenu_About,
        CANTransceiver_SceneMainMenu_SubmenuCallback,
        pInstance);

    submenu_set_selected_item(
        pInstance->pSubmenu,
        scene_manager_get_scene_state(pInstance->pSceneManager, CANTransceiver_SceneMainMenu));
    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewSubmenu);
}

bool CANTransceiver_SceneMainMenu_OnEvent(void* pContext, SceneManagerEvent event) {
    CANTransceiverGlobals* pInstance;
    bool bSuccess;

    pInstance = (CANTransceiverGlobals*)pContext;
    bSuccess = false;

    // Handle the menu item selection events.
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            pInstance->pSceneManager, CANTransceiver_SceneMainMenu, event.event);
        switch(event.event) {
        case CANTransceiver_SceneMainMenu_Receive:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"Read\"");
            scene_manager_next_scene(pInstance->pSceneManager, CANTransceiver_SceneCANRx);
            bSuccess = true;
            break;
        case CANTransceiver_SceneMainMenu_Saved:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"Saved\"");
            // TODO
            bSuccess = true;
            break;
        case CANTransceiver_SceneMainMenu_Transmit:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"Transmit\"");
            scene_manager_next_scene(pInstance->pSceneManager, CANTransceiver_SceneCANTx);
            bSuccess = true;
            break;
        case CANTransceiver_SceneMainMenu_OBDIIMode:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"OBD-II Mode\"");
            // TODO
            bSuccess = true;
            break;
        case CANTransceiver_SceneMainMenu_Setup:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"Setup\"");
            scene_manager_next_scene(pInstance->pSceneManager, CANTransceiver_SceneSetup);
            bSuccess = true;
            break;
        case CANTransceiver_SceneMainMenu_Wiring:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"Wiring\"");
            scene_manager_next_scene(pInstance->pSceneManager, CANTransceiver_SceneWiring);
            bSuccess = true;
            break;
        case CANTransceiver_SceneMainMenu_About:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Selected \"About\"");
            scene_manager_next_scene(pInstance->pSceneManager, CANTransceiver_SceneAbout);
            bSuccess = true;
            break;
        default:
            FURI_LOG_I("CAN-Transceiver", "MainMenu: Unknown selection");
            bSuccess = true;
            break;
        }
    }

    return bSuccess;
}

void CANTransceiver_SceneMainMenu_OnExit(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    submenu_reset(pInstance->pSubmenu);
}
