// About scene. Displays author information.

// ==== INCLUDES ====

#include "../can_transceiver.h"

// ==== CONSTANTS ====

// "About" format ripped from the SPI Mem Manager app's source code.

#define szCAN_TRANSCEIVER_VERSION     "DEV-0.0.1"
#define szCAN_TRANSCEIVER_DEVELOPER   "Chainmanner"
#define szCAN_TRANSCEIVER_ONLINE_REPO "[No online repo yet]"
#define szCAN_TRANSCEIVER_APPNAME     "\e#\e!       CAN Transceiver        \e!\n"
#define szCAN_TRANSCEIVER_BLANKLINE \
    "\e#\e!                                                      \e!\n"

// ==== HANDLERS AND CALLBACKS ====

void CANTransceiver_SceneAbout_OnEnter(void* pContext) {
    CANTransceiverGlobals* pInstance;
    FuriString* pTempStr;

    pInstance = (CANTransceiverGlobals*)pContext;
    pTempStr = furi_string_alloc();

    widget_add_text_box_element(
        pInstance->pWidget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        szCAN_TRANSCEIVER_BLANKLINE,
        false);
    widget_add_text_box_element(
        pInstance->pWidget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        szCAN_TRANSCEIVER_APPNAME,
        false);

    furi_string_printf(pTempStr, "\e#%s\n", "Information");
    furi_string_cat_printf(pTempStr, "Version: %s\n", szCAN_TRANSCEIVER_VERSION);
    furi_string_cat_printf(pTempStr, "Main Developer: %s\n", szCAN_TRANSCEIVER_DEVELOPER);
    furi_string_cat_printf(pTempStr, "Repo: %s\n", szCAN_TRANSCEIVER_ONLINE_REPO);

    furi_string_cat_printf(pTempStr, "\n\e#%s\n", "Description");
    furi_string_cat_printf(
        pTempStr,
        "Interacts with the MCP2515 CAN transceiver via SPI.\n"
        "This is my first Flipper Zero app. Suggestions and criticism are welcome!\n\n"
        "Code takes heavy reference from the SPI Mem Manager app by DrunkBatya.\n"
        "See https://github.com/flipperdevices/flipperzero-good-faps for its source code.\n\n");
    widget_add_text_scroll_element(
        pInstance->pWidget, 0, 16, 128, 50, furi_string_get_cstr(pTempStr));

    furi_string_free(pTempStr);
    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewWidget);
}

bool CANTransceiver_SceneAbout_OnEvent(void* pContext, SceneManagerEvent event) {
    UNUSED(pContext);
    UNUSED(event);
    return false;
}

void CANTransceiver_SceneAbout_OnExit(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    widget_reset(pInstance->pWidget);
}
