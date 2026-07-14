// CAN message transmission scene.

// ==== INCLUDES ====

// App specific
#include "../can_transceiver.h"
#include "../can_transceiver_globalconf.h"
#include "../views/can_transceiver_view_cantx.h"
#include "../lib/mcp2515/mcp2515_spi_worker.h"

// ==== PROTOTYPES ====

void CANTransceiver_SceneCANTx_ResetGlobalsAndView(CANTransceiverGlobals* pInstance);

// ==== HANDLERS AND CALLBACKS ====

void CANTransceiver_SceneCANTx_ViewEventCallback(
    CANTransceiver_ViewCANTx_Event event,
    void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    view_dispatcher_send_custom_event(pInstance->pViewDispatcher, event);
}

void CANTransceiver_SceneCANTx_WorkerCallback(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Update the view model based on the MCP2515's status.
    // Doing so should automatically redraw the view.
    CANTransceiver_ViewCANTx_SetTEC(pInstance->pViewCANTx, pInstance->pMCP2515Worker->u8TEC);
    CANTransceiver_ViewCANTx_SetBusOff(pInstance->pViewCANTx, pInstance->pMCP2515Worker->bBusOff);

    // TODO: If we're unlucky enough to end up in bus-off state, we ought to wait until we get back into bus-on.
    // Now do we do that by giving the worker a task, or by waiting for an interrupt from the MCP2515?
    // Because at this point, we can either wait for the recovery sequence, or we can reset the device ourselves.
}

void CANTransceiver_ViewCANTx_ByteInputCallback_FrameID(void* pContext) {
    CANTransceiverGlobals* pInstance;
    uint32_t u32Temp;
    uint8_t* u8pSrc;
    uint8_t* u8pDst;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Swap the endianness of u32FrameID, we wrote to it as a buffer of bytes but this ARM MCU is little-endian.
    // The swap will be different depending on whether 2 or 4 bytes were inputted.
    // Then AND-op the frame ID with the 11/29-bit mask in case mistakes were made.
    // It's normally be better to warn the user they messed up, but they'll see the difference in the Tx view.
    // After that, set the frame ID global variable and update the view model.
    u32Temp = 0;
    u8pSrc = (uint8_t*)&pInstance->u32FrameID;
    u8pDst = (uint8_t*)&u32Temp;
    if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_29) {
        u8pDst[3] = u8pSrc[0];
        u8pDst[2] = u8pSrc[1];
        u8pDst[1] = u8pSrc[2];
        u8pDst[0] = u8pSrc[3];
        u32Temp &= 0x1FFFFFFF;
        pInstance->u32FrameID = u32Temp;
        CANTransceiver_ViewCANTx_SetFrameID(pInstance->pViewCANTx, pInstance->u32FrameID, 29);
    } else if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_11) {
        u8pDst[1] = u8pSrc[0];
        u8pDst[0] = u8pSrc[1];
        u32Temp &= 0x000007FF;
        pInstance->u32FrameID = u32Temp;
        CANTransceiver_ViewCANTx_SetFrameID(pInstance->pViewCANTx, pInstance->u32FrameID, 11);
    } else {
        ASSERT(false);
    }

    FURI_LOG_I("CAN-Transceiver", "Frame ID updated to %lx", pInstance->u32FrameID);

    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewCANTx);
}

void CANTransceiver_ViewCANTx_ByteInputCallback_FrameData(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    FURI_LOG_I(
        "CAN-Transceiver",
        "Frame data updated to %hhx%hhx%hhx%hhx%hhx%hhx%hhx%hhx (length = %hhd)",
        pInstance->u8pFrameData[0],
        pInstance->u8pFrameData[1],
        pInstance->u8pFrameData[2],
        pInstance->u8pFrameData[3],
        pInstance->u8pFrameData[4],
        pInstance->u8pFrameData[5],
        pInstance->u8pFrameData[6],
        pInstance->u8pFrameData[7],
        pInstance->pGlobalConfig->u8NumDataBytes);

    // Update the view model.
    CANTransceiver_ViewCANTx_SetFrameData(
        pInstance->pViewCANTx, pInstance->u8pFrameData, pInstance->pGlobalConfig->u8NumDataBytes);

    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewCANTx);
}

void CANTransceiver_SceneCANTx_OnEnter(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Reset the view and globals to a clean state.
    CANTransceiver_SceneCANTx_ResetGlobalsAndView(pInstance);

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

    view_dispatcher_switch_to_view(pInstance->pViewDispatcher, CANTransceiverViewCANTx);
}

bool CANTransceiver_SceneCANTx_OnEvent(void* pContext, SceneManagerEvent event) {
    CANTransceiverGlobals* pInstance;
    uint8_t u8ByteInputSize;
    bool bSuccess;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);
    bSuccess = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case CANTransceiver_ViewCANTx_EventEditFrameID:
            FURI_LOG_I("CAN-Transceiver", "Event: Edit Frame ID");
            // TODO: Is there a better way I can do this? Right now I'm using a uint32_t as a buffer.
            // The callback handles translation to the expected format just fine, but when reentering the prompt,
            // the data's shown with the wrong endianness.
            // Not a big deal, but for usability it's not ideal.
            if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_29) {
                byte_input_set_header_text(pInstance->pByteInput, "Frame ID (0x1FFFFFFF)");
                u8ByteInputSize = 4;
            } else if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_11) {
                byte_input_set_header_text(pInstance->pByteInput, "Frame ID (0x7FF)");
                u8ByteInputSize = 2;
            } else {
                ASSERT(false);
                u8ByteInputSize = 0;
            }
            byte_input_set_result_callback(
                pInstance->pByteInput,
                CANTransceiver_ViewCANTx_ByteInputCallback_FrameID,
                NULL,
                pInstance,
                (uint8_t*)&pInstance->u32FrameID,
                u8ByteInputSize);
            // FIXME: Pressing the back button in this view takes us back to the main menu instead of the Tx UI.
            view_dispatcher_switch_to_view(
                pInstance->pViewDispatcher, CANTransceiverViewByteInput);
            bSuccess = true;
            break;
        case CANTransceiver_ViewCANTx_EventEditFrameData:
            FURI_LOG_I("CAN-Transceiver", "Event: Edit Frame Data");
            byte_input_set_header_text(pInstance->pByteInput, "Frame Data");
            u8ByteInputSize = pInstance->pGlobalConfig->u8NumDataBytes;
            byte_input_set_result_callback(
                pInstance->pByteInput,
                CANTransceiver_ViewCANTx_ByteInputCallback_FrameData,
                NULL,
                pInstance,
                pInstance->u8pFrameData,
                u8ByteInputSize);
            // FIXME: Pressing the back button in this view takes us back to the main menu instead of the Tx UI.
            view_dispatcher_switch_to_view(
                pInstance->pViewDispatcher, CANTransceiverViewByteInput);
            bSuccess = true;
            break;
        case CANTransceiver_ViewCANTx_EventResetMCP2515:
            FURI_LOG_I("CAN-Transceiver", "Event: Reset MCP2515");
            // Reset the transceiver. No need to reset the variables on the Flipper's side; we set them on the
            // device only when we're about to do something with it.
            MCP2515Worker_GiveTask(
                pInstance->pMCP2515Worker, u32MCP2515_WORKER_TASK_INIT, NULL, NULL);
            CANTransceiver_SceneCANTx_ResetGlobalsAndView(pInstance);
            bSuccess = true;
            break;
            // REMOVED: Useless feature. At least for me it is.
#if 0
			case CANTransceiver_ViewCANTx_EventSaveMessage:
				// TODO
				bSuccess = true;
				break;
#endif
        case CANTransceiver_ViewCANTx_EventSendMessage:
            FURI_LOG_I("CAN-Transceiver", "Event: Send CAN Frame");
            // If the worker's doing something, we really shouldn't modify its state.
            if(pInstance->pMCP2515Worker->u32CurTask != u32MCP2515_WORKER_TASK_NONE) break;
            // Set the variables for the worker.
            pInstance->pMCP2515Worker->u32FrameID = pInstance->u32FrameID;
            if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_29) {
                pInstance->pMCP2515Worker->u8FrameIDLen = 29;
            } else if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_11) {
                pInstance->pMCP2515Worker->u8FrameIDLen = 11;
            } else {
                ASSERT(false);
            }
            pInstance->pMCP2515Worker->u8FrameDataLen = pInstance->pGlobalConfig->u8NumDataBytes;
            pInstance->pMCP2515Worker->bOneShot = pInstance->pGlobalConfig->bTxOneShot;
            memcpy(
                pInstance->pMCP2515Worker->u8pFrameData,
                pInstance->u8pFrameData,
                pInstance->pGlobalConfig->u8NumDataBytes);
            // Now send the message!
            MCP2515Worker_GiveTask(
                pInstance->pMCP2515Worker,
                u32MCP2515_WORKER_TASK_TX,
                CANTransceiver_SceneCANTx_WorkerCallback,
                pInstance);
            bSuccess = true;
            break;
        default:
            break;
        }
    }

    return bSuccess;
}

void CANTransceiver_SceneCANTx_OnExit(void* pContext) {
    CANTransceiverGlobals* pInstance;

    pInstance = (CANTransceiverGlobals*)pContext;
    ASSERT(pInstance != NULL);

    // Stop the worker thread. Nothing else needs to be done here.
    MCP2515Worker_StopWorkerThread(pInstance->pMCP2515Worker);
}

// ==== PRIVATE FUNCTIONS ====

void CANTransceiver_SceneCANTx_ResetGlobalsAndView(CANTransceiverGlobals* pInstance) {
    uint8_t u8pNullArray[8];

    ASSERT(pInstance != NULL);

    // Reset the global Tx state variables.
    pInstance->u32FrameID = 0;
    memset(pInstance->u8pFrameData, 0, 8);

    // Reset the view.
    if(pInstance->pGlobalConfig->u8FrameIDLen == u8FRAME_ID_LEN_29) {
        CANTransceiver_ViewCANTx_SetFrameID(pInstance->pViewCANTx, 0, 29);
    } else {
        CANTransceiver_ViewCANTx_SetFrameID(pInstance->pViewCANTx, 0, 11);
    }
    memset(u8pNullArray, 0, 8);
    CANTransceiver_ViewCANTx_SetFrameData(
        pInstance->pViewCANTx, u8pNullArray, pInstance->pGlobalConfig->u8NumDataBytes);
    CANTransceiver_ViewCANTx_SetTEC(pInstance->pViewCANTx, 0);
    CANTransceiver_ViewCANTx_SetBusOff(pInstance->pViewCANTx, 0);
    CANTransceiver_ViewCANTx_SetEventCallback(
        pInstance->pViewCANTx, CANTransceiver_SceneCANTx_ViewEventCallback, pInstance);
}
