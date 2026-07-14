// CAN transceiver configuration scene.

// NOTE: The Signal Generator app's source code was used as reference here.
// It's also in the flipperzero-good-faps repo.

// ==== INCLUDES ====

#include "../can_transceiver.h"

// ==== TYPEDEFS ====

typedef enum {
    CANTransceiver_SceneSetup_BaudRate,
    CANTransceiver_SceneSetup_XtalFreq,
    CANTransceiver_SceneSetup_SleepTimeout,
    CANTransceiver_SceneSetup_ListenOnlyMode,
    //	CANTransceiver_SceneSetup_Filters,
    CANTransceiver_SceneSetup_FrameIDLen,
    CANTransceiver_SceneSetup_NumDataBytes,
    CANTransceiver_SceneSetup_TxOneShot,
} CANTransceiver_SceneSetup_Index;

// ==== CONSTANTS ====

static const char* const szpOffOrOnTexts[] = {
    "Off",
    "On",
};

// The indices of each menu item needs to correspond with the value of each option in can_transceiver_globalconf.h!

// Corresponds to u8pBaudRates.
static const char* const szpBaudRateTexts[] = {
    "125 kbps",
    "250 kbps",
    "500 kbps",
    "1 Mbps",
    //	"Auto",	// TODO: NYI
};

// Corresponds to OscillatorFrequencies.
static const char* const szpOscillatorFrequencyTexts[] = {
    "16 MHz",
};

// Corresponds to u16pSleepTimeouts_sec.
// XXX: To hell with this. It may come in useful occasionally, but for my purposes it likely won't.
#if 0
static const char* const szpSleepTimeoutTexts[] = {
	"Never",
	"5 secs",
	"10 secs",
	"30 secs",
	"60 secs",
	"120 secs",
};
#endif

// TODO: Eventually add support for Rx filters and masks.

// Corresponds to FrameIDBitsOptions.
static const char* const szpFrameIDBitsTexts[] = {
    "11",
    "29",
};

// Corresponds to u8pNumDataBytesOptions.
static const char* const szpNumDataBytesTexts[] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
};

// ==== HANDLERS AND CALLBACKS ====

// Is there a better way of doing this than having several callbacks for each menu item?
// I have one in mind now, but damn it, I'm in too deep.
static void CANTransceiver_SceneSetup_ChangeCallback_BaudRate(VariableItem* pVarItem) {
    CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
    uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

    variable_item_set_current_value_text(pVarItem, szpBaudRateTexts[u8Index]);
    pInstance->pGlobalConfig->u8BaudRate = u8Index;

    view_dispatcher_send_custom_event(
        pInstance->pViewDispatcher, CANTransceiver_SceneSetup_BaudRate);
}
static void CANTransceiver_SceneSetup_ChangeCallback_XtalFreq(VariableItem* pVarItem) {
    CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
    uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

    variable_item_set_current_value_text(pVarItem, szpOscillatorFrequencyTexts[u8Index]);
    pInstance->pGlobalConfig->u8XtalFreq = u8Index;

    view_dispatcher_send_custom_event(
        pInstance->pViewDispatcher, CANTransceiver_SceneSetup_XtalFreq);
}
#if 0
// REMOVED: See above for why.
static void CANTransceiver_SceneSetup_ChangeCallback_SleepTimeout(VariableItem* pVarItem)
{
	CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
	uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

	variable_item_set_current_value_text(pVarItem, szpSleepTimeoutTexts[u8Index]);
	pInstance->pGlobalConfig->u8SleepTimeout = u8Index;

	view_dispatcher_send_custom_event(pInstance->pViewDispatcher, CANTransceiver_SceneSetup_SleepTimeout);
}
#endif
static void CANTransceiver_SceneSetup_ChangeCallback_ListenOnlyMode(VariableItem* pVarItem) {
    CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
    uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

    variable_item_set_current_value_text(pVarItem, szpOffOrOnTexts[u8Index]);
    pInstance->pGlobalConfig->bListenOnly = u8Index; // u8Index is zero or one, nothing else.

    view_dispatcher_send_custom_event(
        pInstance->pViewDispatcher, CANTransceiver_SceneSetup_ListenOnlyMode);
}
// TODO: Filter-related stuff here.
static void CANTransceiver_SceneSetup_ChangeCallback_FrameIDLen(VariableItem* pVarItem) {
    CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
    uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

    variable_item_set_current_value_text(pVarItem, szpFrameIDBitsTexts[u8Index]);
    pInstance->pGlobalConfig->u8FrameIDLen = u8Index;

    view_dispatcher_send_custom_event(
        pInstance->pViewDispatcher, CANTransceiver_SceneSetup_FrameIDLen);
}
static void CANTransceiver_SceneSetup_ChangeCallback_NumDataBytes(VariableItem* pVarItem) {
    CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
    uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

    variable_item_set_current_value_text(pVarItem, szpNumDataBytesTexts[u8Index]);
    pInstance->pGlobalConfig->u8NumDataBytes = u8Index; // Value's the same as the text.

    view_dispatcher_send_custom_event(
        pInstance->pViewDispatcher, CANTransceiver_SceneSetup_NumDataBytes);
}
static void CANTransceiver_SceneSetup_ChangeCallback_TxOneShot(VariableItem* pVarItem) {
    CANTransceiverGlobals* pInstance = (CANTransceiverGlobals*)variable_item_get_context(pVarItem);
    uint8_t u8Index = variable_item_get_current_value_index(pVarItem);

    variable_item_set_current_value_text(pVarItem, szpOffOrOnTexts[u8Index]);
    pInstance->pGlobalConfig->bTxOneShot = u8Index; // u8Index is zero or one, nothing else.

    view_dispatcher_send_custom_event(
        pInstance->pViewDispatcher, CANTransceiver_SceneSetup_TxOneShot);
}

void CANTransceiver_SceneSetup_OnEnter(void* pContext) {
    CANTransceiverGlobals* pInstance;
    VariableItem* pCurItem;

    pInstance = (CANTransceiverGlobals*)pContext;

    // Create the menu by adding each item and configuring it.

    // Baud rate
    ASSERT(pInstance->pGlobalConfig->u8BaudRate <= u8BAUD_RATE_1Mbps);
    pCurItem = variable_item_list_add(
        pInstance->pVarItemList,
        "Baud Rate",
        COUNT_OF(szpBaudRateTexts),
        CANTransceiver_SceneSetup_ChangeCallback_BaudRate,
        pContext);
    variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->u8BaudRate);
    variable_item_set_current_value_text(
        pCurItem, szpBaudRateTexts[pInstance->pGlobalConfig->u8BaudRate]);

    // Crystal oscillator frequency
    ASSERT(pInstance->pGlobalConfig->u8XtalFreq <= u8XTAL_FREQ_16MHz);
    pCurItem = variable_item_list_add(
        pInstance->pVarItemList,
        "Osc. Freq.",
        COUNT_OF(szpOscillatorFrequencyTexts),
        CANTransceiver_SceneSetup_ChangeCallback_XtalFreq,
        pContext);
    variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->u8XtalFreq);
    variable_item_set_current_value_text(
        pCurItem, szpOscillatorFrequencyTexts[pInstance->pGlobalConfig->u8XtalFreq]);

    // Timeout before sleep
    // REMOVED: See above for why.
#if 0
	ASSERT(pInstance->pGlobalConfig->u8SleepTimeout <= u8SLEEP_TIMEOUT_120s);
	pCurItem = variable_item_list_add(
			pInstance->pVarItemList,
			"Sleep after",
			COUNT_OF(szpSleepTimeoutTexts),
			CANTransceiver_SceneSetup_ChangeCallback_SleepTimeout,
			pContext);
	variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->u8SleepTimeout);
	variable_item_set_current_value_text(pCurItem, szpSleepTimeoutTexts[pInstance->pGlobalConfig->u8SleepTimeout]);
#endif

    // Listen-only mode
    ASSERT(pInstance->pGlobalConfig->bListenOnly <= 1);
    pCurItem = variable_item_list_add(
        pInstance->pVarItemList,
        "Silent Mode",
        2,
        CANTransceiver_SceneSetup_ChangeCallback_ListenOnlyMode,
        pContext);
    variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->bListenOnly);
    variable_item_set_current_value_text(
        pCurItem, szpOffOrOnTexts[pInstance->pGlobalConfig->bListenOnly]);

    // TODO: Filter options.

    // CAN frame type
    ASSERT(pInstance->pGlobalConfig->u8FrameIDLen <= u8FRAME_ID_LEN_29);
    pCurItem = variable_item_list_add(
        pInstance->pVarItemList,
        "Frame Bits",
        COUNT_OF(szpFrameIDBitsTexts),
        CANTransceiver_SceneSetup_ChangeCallback_FrameIDLen,
        pContext);
    variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->u8FrameIDLen);
    variable_item_set_current_value_text(
        pCurItem, szpFrameIDBitsTexts[pInstance->pGlobalConfig->u8FrameIDLen]);

    // Number of data bytes
    ASSERT(pInstance->pGlobalConfig->u8NumDataBytes <= 8);
    pCurItem = variable_item_list_add(
        pInstance->pVarItemList,
        "Data Bytes",
        COUNT_OF(szpNumDataBytesTexts),
        CANTransceiver_SceneSetup_ChangeCallback_NumDataBytes,
        pContext);
    variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->u8NumDataBytes);
    variable_item_set_current_value_text(
        pCurItem, szpNumDataBytesTexts[pInstance->pGlobalConfig->u8NumDataBytes]);

    // One-shot transmission mode
    ASSERT(pInstance->pGlobalConfig->bTxOneShot <= 1);
    pCurItem = variable_item_list_add(
        pInstance->pVarItemList,
        "One-Shot Tx",
        2,
        CANTransceiver_SceneSetup_ChangeCallback_TxOneShot,
        pContext);
    variable_item_set_current_value_index(pCurItem, pInstance->pGlobalConfig->bTxOneShot);
    variable_item_set_current_value_text(
        pCurItem, szpOffOrOnTexts[pInstance->pGlobalConfig->bTxOneShot]);

    variable_item_list_set_selected_item(
        pInstance->pVarItemList, CANTransceiver_SceneSetup_BaudRate);
    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewVariableItemList);
}

bool CANTransceiver_SceneSetup_OnEvent(void* pContext, SceneManagerEvent event) {
    UNUSED(pContext);
    bool bSuccess;

    bSuccess = false;

    if(event.type == SceneManagerEventTypeCustom) {
        bSuccess = true; // TODO: Log the events or something, I don't know.
    }

    return bSuccess;
}

void CANTransceiver_SceneSetup_OnExit(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    variable_item_list_reset(pInstance->pVarItemList);
}
