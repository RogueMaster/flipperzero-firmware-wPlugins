// CAN message reception and history scene.

// ==== INCLUDES ====

// App specific
#include "../can_transceiver.h"
#include "../can_transceiver_globalconf.h"
#include "../lib/mcp2515/mcp2515_spi_worker.h"
#include "../views/can_transceiver_view_canrx.h"

// ==== CONSTANTS ====

#define u32DELAY_BEFORE_UI_REFRESH 500 // In ticks, and I think 1 tick = 1 ms.

// ==== FUNCTION PROTOTYPES ====

void CANTransceiver_SceneCANRx_FilenameEnteredCallback(void* pContext);

// ==== PRIVATE FUNCTIONS ====

void CANTransceiver_SceneCANRx_ResetHistory(CANTransceiverGlobals* pInstance) {
    ASSERT(pInstance != NULL);

    // Reset the UI model.
    CANTransceiver_ViewCANRx_SetHistoryNumElements(pInstance->pViewCANRx, 0);
    CANTransceiver_ViewCANRx_SetHistoryIndex(pInstance->pViewCANRx, 0);
    CANTransceiver_ViewCANRx_SetHistoryListOffset(pInstance->pViewCANRx, 0);
    CANTransceiver_ViewCANRx_SetREC(pInstance->pViewCANRx, 0);
    CANTransceiver_ViewCANRx_SetRxHistoryPtr(pInstance->pViewCANRx, pInstance->pRxHistory);
    CANTransceiver_ViewCANRx_RefreshUI(pInstance->pViewCANRx);

    // Clear the actual stored values. Do this last in case the MCP2515 thread's active and still receiving.
    pInstance->u32LastUIRefreshTime = furi_get_tick();
    CANTransceiver_RxHistory_Reset(pInstance->pRxHistory);
    pInstance->u16HistoryListOffset = 0;
    pInstance->u16HistoryIndex = 0;
    pInstance->u16HistoryNumElements = 0;
}

void CANTransceiver_SceneCANRx_RefreshUI(CANTransceiverGlobals* pInstance) {
    pInstance->u32LastUIRefreshTime = furi_get_tick();
    CANTransceiver_ViewCANRx_SetHistoryNumElements(
        pInstance->pViewCANRx, pInstance->u16HistoryNumElements);
    CANTransceiver_ViewCANRx_SetHistoryIndex(pInstance->pViewCANRx, pInstance->u16HistoryIndex);
    CANTransceiver_ViewCANRx_SetHistoryListOffset(
        pInstance->pViewCANRx, pInstance->u16HistoryListOffset);
    CANTransceiver_ViewCANRx_SetREC(pInstance->pViewCANRx, pInstance->pMCP2515Worker->u8REC);
    CANTransceiver_ViewCANRx_RefreshUI(pInstance->pViewCANRx);
}

// NOTE: Probably better to be in can_transceiver_view_canrx.c.
void CANTransceiver_SceneCANRx_SelectNextItem(CANTransceiverGlobals* pInstance) {
    // Don't do it if there are no elements or the current element is the last one.
    if(pInstance->u16HistoryNumElements == 0) return;
    if(pInstance->u16HistoryIndex == pInstance->u16HistoryNumElements - 1) return;

    // Increase the index, and the list offset if we're going past the floor.
    pInstance->u16HistoryIndex++;
    if(pInstance->u16HistoryIndex - pInstance->u16HistoryListOffset >= CANRX_MAX_MSGS_PER_SCREEN)
        pInstance->u16HistoryListOffset++;
}

// NOTE: Probably better to be in can_transceiver_view_canrx.c.
void CANTransceiver_SceneCANRx_SelectPrevItem(CANTransceiverGlobals* pInstance) {
    // Don't do it if there are no elements or the current element is the first one.
    if(pInstance->u16HistoryNumElements == 0) return;
    if(pInstance->u16HistoryIndex == 0) return;

    // Decrease the index, and the list offset if we're going past the ceiling.
    pInstance->u16HistoryIndex--;
    if(pInstance->u16HistoryIndex < pInstance->u16HistoryListOffset)
        pInstance->u16HistoryListOffset = pInstance->u16HistoryIndex;
}

void CANTransceiver_SceneCANRx_SaveFrameCap(void* pContext) {
    CANTransceiverGlobals* pInstance;
    char* i8pFilePath;
    File* pFile;
    char* i8pLine;
    CANTransceiver_RxHistory_Entry* pCurEntry;
    char i8pTempBuf[32];
    uint16_t i;
    uint8_t j;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);
    i8pFilePath = malloc(256);
    pFile = storage_file_alloc(pInstance->pFSApi);

    // Temporarily stop the Rx thread, just to be safe.
    // FIXME: I don't think this is working. It still seems as if there's a task going on.
    //MCP2515Worker_StopWorkerThread(pInstance->pMCP2515Worker);

    snprintf(
        i8pFilePath, 256, "%s/%s.csv", STORAGE_APP_DATA_PATH_PREFIX, pInstance->i8pLogFilename);
    FURI_LOG_I("CAN-Transceiver", "Opening file %s", i8pFilePath);
    // Open the file.
    if(!storage_file_open(pFile, i8pFilePath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_I("CAN-Transceiver", "Cannot open!");
        goto FilenameEnteredCallback_end;
    }
    // Write the header.
    if(!storage_file_write(
           pFile, "timestamp,frame_id,frame_data,frame_data_len,is_extended,is_remote\n", 67)) {
        FURI_LOG_I("CAN-Transceiver", "Failed to write header!");
        goto FilenameEnteredCallback_end;
    }
    // Now write each message to the log file.
    i8pLine = malloc(128);
    for(i = 0; i < pInstance->u16HistoryNumElements; i++) {
        memset(i8pLine, 0, 128);
        memset(i8pTempBuf, 0, 32);
        pCurEntry = CANTransceiver_RxHistory_GetEntry(pInstance->pRxHistory, i);

        // The data could be of variable length, so write it to a temporary buffer.
        for(j = 0; j < pCurEntry->u4FrameDataLen; j++) {
            snprintf(i8pTempBuf + (j * 2), 3, "%02hhx", pCurEntry->u8pFrameData[j]);
        }
        // Create the line. Two ways it can go, depending on whether the ID is extended or not.
        if(pCurEntry->bExtendedIDFrame) {
            snprintf(
                i8pLine,
                128,
                "%ld,%08lx,%s,%hhd,%hhd,%hhd\n",
                pCurEntry->u32Timestamp,
                pCurEntry->u32FrameID,
                i8pTempBuf,
                pCurEntry->u4FrameDataLen,
                pCurEntry->bExtendedIDFrame,
                pCurEntry->bRemoteTransferFrame);
        } else {
            snprintf(
                i8pLine,
                128,
                "%ld,%04lx,%s,%hhd,%hhd,%hhd\n",
                pCurEntry->u32Timestamp,
                pCurEntry->u32FrameID,
                i8pTempBuf,
                pCurEntry->u4FrameDataLen,
                pCurEntry->bExtendedIDFrame,
                pCurEntry->bRemoteTransferFrame);
        }
        FURI_LOG_I("CAN-Transceiver", "Current line: %s", i8pLine);
        // Write the line to the file.
        if(!storage_file_write(pFile, i8pLine, strlen(i8pLine))) {
            FURI_LOG_I("CAN-Transceiver", "Failed to write entry %d!", i);
            break;
        }
    }
    free(i8pLine);

FilenameEnteredCallback_end:
    storage_file_close(pFile);
    storage_file_free(pFile);
    free(i8pFilePath);
    //MCP2515Worker_GiveTask(pInstance->pMCP2515Worker, u32MCP2515_WORKER_TASK_RX, NULL, NULL);

    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewCANRx);
}

void CANTransceiver_SceneCANRx_SaveCapToFile(CANTransceiverGlobals* pInstance) {
    ValidatorIsFile* pValidator;

    ASSERT(pInstance != NULL);

    pValidator = validator_is_file_alloc_init(STORAGE_APP_DATA_PATH_PREFIX, ".csv", "");
    text_input_set_validator(pInstance->pTextInput, validator_is_file_callback, pValidator);

    text_input_set_header_text(pInstance->pTextInput, "Export filename");
    text_input_set_result_callback(
        pInstance->pTextInput,
        CANTransceiver_SceneCANRx_FilenameEnteredCallback,
        pInstance,
        pInstance->i8pLogFilename,
        64,
        true);
    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewTextInput);
}

// ==== HANDLERS AND CALLBACKS ====

void CANTransceiver_SceneCANRx_FilenameEnteredCallback(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // TODO: I hate using a hardcoded constants. I hope no other event has the same number...
    view_dispatcher_send_custom_event(pInstance->pViewDispatcher, 0x43211234);
}

void CANTransceiver_SceneCANRx_ViewEventCallback(
    CANTransceiver_ViewCANRx_Event event,
    void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    view_dispatcher_send_custom_event(pInstance->pViewDispatcher, event);
}

void CANTransceiver_SceneCANRx_MessageRxCallback(void* pContext) {
    CANTransceiverGlobals* pInstance;
    CANTransceiver_RxHistory_Entry pNewEntry;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Add the message if this is not an error frame.
    if(!pInstance->pMCP2515Worker->bErrorOccurred &&
       pInstance->u16HistoryNumElements < u16RXHISTORY_MAX_NUM_MSG) {
        pNewEntry.u32FrameID = pInstance->pMCP2515Worker->u32FrameID_Rx & 0x1FFFFFFF;
        if(pInstance->pMCP2515Worker->u32FrameID_Rx & 0x80000000)
            pNewEntry.bExtendedIDFrame = true;
        if(pInstance->pMCP2515Worker->u32FrameID_Rx & 0x40000000)
            pNewEntry.bRemoteTransferFrame = true;
        pNewEntry.u8pFrameData[0] = pInstance->pMCP2515Worker->u8pFrameData_Rx[0];
        pNewEntry.u8pFrameData[1] = pInstance->pMCP2515Worker->u8pFrameData_Rx[1];
        pNewEntry.u8pFrameData[2] = pInstance->pMCP2515Worker->u8pFrameData_Rx[2];
        pNewEntry.u8pFrameData[3] = pInstance->pMCP2515Worker->u8pFrameData_Rx[3];
        pNewEntry.u8pFrameData[4] = pInstance->pMCP2515Worker->u8pFrameData_Rx[4];
        pNewEntry.u8pFrameData[5] = pInstance->pMCP2515Worker->u8pFrameData_Rx[5];
        pNewEntry.u8pFrameData[6] = pInstance->pMCP2515Worker->u8pFrameData_Rx[6];
        pNewEntry.u8pFrameData[7] = pInstance->pMCP2515Worker->u8pFrameData_Rx[7];
        pNewEntry.u4FrameDataLen = pInstance->pMCP2515Worker->u8FrameDataLen_Rx & 0x0F;
        pNewEntry.u32Timestamp = furi_get_tick();
        CANTransceiver_RxHistory_AddEntry(pInstance->pRxHistory, &pNewEntry);
        pInstance->u16HistoryNumElements++;

        // If we've selected the last element, go down one.
        if(pInstance->u16HistoryIndex == pInstance->u16HistoryNumElements - 2) {
            CANTransceiver_SceneCANRx_SelectNextItem(pInstance);
        }
    }

    // Refresh the UI, but not too quickly! It causes a decent amount of lag.
    // TODO: Even this might not be enough. I've been thinking about adding another thread specifically to refresh the UI.
    // That thread would be of lower priority, and this here would just signal for it to run.
    // That'll make this a lot faster, particularly important since this function's in the MCP2515 worker thread context.
    if(furi_get_tick() >= pInstance->u32LastUIRefreshTime + u32DELAY_BEFORE_UI_REFRESH) {
        CANTransceiver_SceneCANRx_RefreshUI(pInstance);
    }
}

void CANTransceiver_SceneCANRx_OnEnter(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Reset the view and history to a clean state.
    CANTransceiver_SceneCANRx_ResetHistory(pInstance);
    CANTransceiver_ViewCANRx_SetEventCallback(
        pInstance->pViewCANRx, CANTransceiver_SceneCANRx_ViewEventCallback, pInstance);

    // Start the thread and reset the transceiver to ensure a clean state, then wait for it to finish.
    MCP2515Worker_StartThread(pInstance->pMCP2515Worker);
    pInstance->pMCP2515Worker->u8CNF1 =
        u8ppCANBaudRates_16MHz[pInstance->pGlobalConfig->u8BaudRate][0];
    pInstance->pMCP2515Worker->u8CNF2 =
        u8ppCANBaudRates_16MHz[pInstance->pGlobalConfig->u8BaudRate][1];
    pInstance->pMCP2515Worker->u8CNF3 =
        u8ppCANBaudRates_16MHz[pInstance->pGlobalConfig->u8BaudRate][2];
    MCP2515Worker_GiveTask(pInstance->pMCP2515Worker, u32MCP2515_WORKER_TASK_INIT, NULL, NULL);
    while(pInstance->pMCP2515Worker->u32CurTask != u32MCP2515_WORKER_TASK_NONE)
        ;

    // Now start receiving!
    pInstance->pMCP2515Worker->bListenOnly = pInstance->pGlobalConfig->bListenOnly;
    pInstance->pMCP2515Worker->pRxCallback = CANTransceiver_SceneCANRx_MessageRxCallback;
    pInstance->pMCP2515Worker->pRxCallbackContext = pInstance;
    MCP2515Worker_GiveTask(pInstance->pMCP2515Worker, u32MCP2515_WORKER_TASK_RX, NULL, NULL);

#if 0
	// FIXME: TESTING RX HISTORY, REMOVE WHEN DONE
	CANTransceiver_RxHistory_Entry curItem;
	pInstance->u16HistoryNumElements = 1000;
	for (int i = 0; i < pInstance->u16HistoryNumElements; i++)
	{
		memset(&curItem, 0, sizeof(CANTransceiver_RxHistory_Entry));
		curItem.u32Timestamp = i;
		curItem.u32FrameID = i;
		memset(curItem.u8pFrameData, (uint8_t)i, 8);
		curItem.u4FrameDataLen = (uint8_t)(i % 8);
		curItem.bExtendedIDFrame = (i % 3 == 0);
		curItem.bRemoteTransferFrame = (i % 5 == 0);
		CANTransceiver_RxHistory_AddEntry(pInstance->pRxHistory, &curItem);
	}
	CANTransceiver_ViewCANRx_SetRxHistoryPtr(pInstance->pViewCANRx, pInstance->pRxHistory);
	CANTransceiver_ViewCANRx_SetHistoryNumElements(pInstance->pViewCANRx, pInstance->u16HistoryNumElements);
	CANTransceiver_ViewCANRx_SetREC(pInstance->pViewCANRx, 254);
#endif

    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewCANRx);
}

bool CANTransceiver_SceneCANRx_OnEvent(void* pContext, SceneManagerEvent event) {
    CANTransceiverGlobals* pInstance;
    bool bSuccess;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);
    bSuccess = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case CANTransceiver_ViewCANRx_EventClearHistory:
            FURI_LOG_I("CAN-Transceiver", "Event: Clear History");
            CANTransceiver_SceneCANRx_ResetHistory(pInstance);
            CANTransceiver_SceneCANRx_RefreshUI(pInstance);
            bSuccess = true;
            break;
        case CANTransceiver_ViewCANRx_EventSaveHistory:
            FURI_LOG_I("CAN-Transceiver", "Event: Save Message History");
            CANTransceiver_SceneCANRx_SaveCapToFile(pInstance);
            break;
        case CANTransceiver_ViewCANRx_EventPrevMsg:
            FURI_LOG_I("CAN-Transceiver", "Event: Select Previous Message");
            CANTransceiver_SceneCANRx_SelectPrevItem(pInstance);
            CANTransceiver_SceneCANRx_RefreshUI(pInstance);
            bSuccess = true;
            break;
        case CANTransceiver_ViewCANRx_EventNextMsg:
            FURI_LOG_I("CAN-Transceiver", "Event: Select Next Message");
            CANTransceiver_SceneCANRx_SelectNextItem(pInstance);
            CANTransceiver_SceneCANRx_RefreshUI(pInstance);
            bSuccess = true;
            break;
        case 0x43211234:
            FURI_LOG_I("CAN-Transceiver", "Event: Save Frame Capture");
            CANTransceiver_SceneCANRx_SaveFrameCap(pContext);
            CANTransceiver_SceneCANRx_RefreshUI(pInstance);
            bSuccess = true;
            break;
        // TODO: Should really have long-press events for quicker scrolling.
        default:
            break;
        }
    }

    return bSuccess;
}

void CANTransceiver_SceneCANRx_OnExit(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Stop the worker thread. Nothing else needs to be done here.
    MCP2515Worker_StopWorkerThread(pInstance->pMCP2515Worker);

    // Reset the history to free the used memory.
    CANTransceiver_RxHistory_Reset(pInstance->pRxHistory);
}
