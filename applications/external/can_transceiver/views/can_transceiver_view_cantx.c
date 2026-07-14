// CAN message transmission view.

// ==== INCLUDES ====

// Libc
#include <stdint.h>

// App-specific
#include "can_transceiver.h"
#include "can_transceiver_view_cantx.h"

// ==== TYPEDEFS ====

typedef struct {
    uint32_t u32FrameID;
    uint8_t u8FrameIDLen; // in bits
    uint8_t u8pFrameData[8];
    uint8_t u8FrameDataLen; // in bytes
    //uint8_t u8MCP2515Status;	// REMOVED: Don't need this all that much. Might add if that changes.
    uint8_t u8TEC;
    uint8_t bBusOff;
} CANTransceiver_ViewCANTx_Model;

// ==== STATIC FUNCTIONS ====

static void CANTransceiver_ViewCANTx_DrawCallback(Canvas* pCanvas, void* pModelVoid) {
    CANTransceiver_ViewCANTx_Model* pModel = pModelVoid;
    char i8pTempBuf[32];
    char i8pTempBuf2[3];
    uint8_t i;

    ASSERT(pCanvas != NULL);
    ASSERT(pModelVoid != NULL);

    canvas_set_color(pCanvas, ColorBlack);
    // Use "FontPrimary" for large text, "FontKeyboard" for smaller text.
    canvas_set_font(pCanvas, FontKeyboard); //FontPrimary);

    // For the "FontKeyboard" font I recommend a vertical spacing of 12 pixels.
    // Note that this font's text cuts off horizontally at 20.5 chars.

    // Frame ID
    memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
    if(pModel->u8FrameIDLen == 29) {
        snprintf(i8pTempBuf, 21, "ID: 0x%08lx", pModel->u32FrameID);
    } else if(pModel->u8FrameIDLen == 11) {
        snprintf(i8pTempBuf, 21, "ID: 0x%04lx", pModel->u32FrameID);
    } else {
        ASSERT(false);
    }
    i8pTempBuf[20] = 0;
    canvas_draw_str(pCanvas, 0, 8, i8pTempBuf);

    // Frame data
    memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
    memcpy(i8pTempBuf, "D: ", sizeof("D: ") - 1);
    for(i = 0; i < pModel->u8FrameDataLen; i++) {
        snprintf(i8pTempBuf2, 3, "%02hhx", pModel->u8pFrameData[i]);
        i8pTempBuf2[2] = 0;
        memcpy(i8pTempBuf + 3 + (i * 2), i8pTempBuf2, 3);
    }
    i8pTempBuf[20] = 0;
    canvas_draw_str(pCanvas, 0, 20, i8pTempBuf);

    // TEC or bus-off status
    if(pModel->bBusOff) {
        canvas_draw_str(pCanvas, 0, 32, "Bus off!");
    } else {
        snprintf(i8pTempBuf, 21, "TEC: %hhd", pModel->u8TEC);
        canvas_draw_str(pCanvas, 0, 32, i8pTempBuf);
    }
}

static bool CANTransceiver_ViewCANTx_InputCallback(InputEvent* pInputEvent, void* pContext) {
    CANTransceiver_ViewCANTx* pViewCANTx;
    CANTransceiver_ViewCANTx_Event eventToForward;
    bool bSuccess;

    ASSERT(pInputEvent != NULL);
    ASSERT(pContext != NULL);

    pViewCANTx = pContext;
    eventToForward = CANTransceiver_ViewCANTx_EventUnknown;
    bSuccess = false;
    if(pInputEvent->type == InputTypeShort) {
        switch(pInputEvent->key) {
        case InputKeyUp:
            eventToForward = CANTransceiver_ViewCANTx_EventEditFrameID;
            break;
        case InputKeyLeft:
            eventToForward = CANTransceiver_ViewCANTx_EventEditFrameData;
            break;
        case InputKeyDown:
            eventToForward = CANTransceiver_ViewCANTx_EventResetMCP2515;
            break;
        case InputKeyRight:
            eventToForward = CANTransceiver_ViewCANTx_EventSaveMessage;
            break;
        case InputKeyOk:
            eventToForward = CANTransceiver_ViewCANTx_EventSendMessage;
            break;
        default:
            break;
        }
    }

    ASSERT(pViewCANTx->pCallback != NULL);
    if(eventToForward != CANTransceiver_ViewCANTx_EventUnknown) {
        bSuccess = true;
        pViewCANTx->pCallback(eventToForward, pViewCANTx->pCallbackContext);
    }

    return bSuccess;
}

static void CANTransceiver_ViewCANTx_EnterCallback(void* pContext) {
    CANTransceiver_ViewCANTx* pViewCANTx = pContext;

    ASSERT(pContext != NULL);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        true);
}

static void CANTransceiver_ViewCANTx_ExitCallback(void* pContext) {
    CANTransceiver_ViewCANTx* pViewCANTx = pContext;

    ASSERT(pContext != NULL);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        false);
}

// ==== PUBLIC FUNCTIONS ====

CANTransceiver_ViewCANTx* CANTransceiver_ViewCANTx_Alloc() {
    CANTransceiver_ViewCANTx* pViewCANTx;

    pViewCANTx = malloc(sizeof(CANTransceiver_ViewCANTx));
    pViewCANTx->pView = view_alloc();
    view_set_context(pViewCANTx->pView, pViewCANTx);
    view_allocate_model(
        pViewCANTx->pView, ViewModelTypeLocking, sizeof(CANTransceiver_ViewCANTx_Model));
    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        false);

    view_set_draw_callback(pViewCANTx->pView, CANTransceiver_ViewCANTx_DrawCallback);
    view_set_enter_callback(pViewCANTx->pView, CANTransceiver_ViewCANTx_EnterCallback);
    view_set_exit_callback(pViewCANTx->pView, CANTransceiver_ViewCANTx_ExitCallback);
    view_set_input_callback(pViewCANTx->pView, CANTransceiver_ViewCANTx_InputCallback);

    return pViewCANTx;
}

void CANTransceiver_ViewCANTx_Free(CANTransceiver_ViewCANTx* pViewCANTx) {
    ASSERT(pViewCANTx != NULL);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        false);

    view_free(pViewCANTx->pView);
    free(pViewCANTx);
}

View* CANTransceiver_ViewCANTx_GetView(CANTransceiver_ViewCANTx* pViewCANTx) {
    ASSERT(pViewCANTx != NULL);

    return pViewCANTx->pView;
}

void CANTransceiver_ViewCANTx_SetFrameID(
    CANTransceiver_ViewCANTx* pViewCANTx,
    uint32_t u32FrameID,
    uint8_t u8FrameIDLen) {
    ASSERT(pViewCANTx != NULL);
    ASSERT(u8FrameIDLen == 11 || u8FrameIDLen == 29);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        {
            pModel->u32FrameID = u32FrameID;
            pModel->u8FrameIDLen = u8FrameIDLen;
        },
        true);
}

void CANTransceiver_ViewCANTx_SetFrameData(
    CANTransceiver_ViewCANTx* pViewCANTx,
    uint8_t* u8pFrameData,
    uint8_t u8FrameDataLen) {
    uint8_t i;

    ASSERT(pViewCANTx != NULL);
    ASSERT(u8pFrameData != NULL);
    ASSERT(u8FrameDataLen <= 8);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        {
            for(i = 0; i < u8FrameDataLen; i++) {
                pModel->u8pFrameData[i] = u8pFrameData[i];
            }
            pModel->u8FrameDataLen = u8FrameDataLen;
        },
        true);
}

void CANTransceiver_ViewCANTx_SetTEC(CANTransceiver_ViewCANTx* pViewCANTx, uint8_t u8TEC) {
    ASSERT(pViewCANTx != NULL);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        { pModel->u8TEC = u8TEC; },
        true);
}

void CANTransceiver_ViewCANTx_SetBusOff(CANTransceiver_ViewCANTx* pViewCANTx, uint8_t bBusOff) {
    ASSERT(pViewCANTx != NULL);

    with_view_model(
        pViewCANTx->pView,
        CANTransceiver_ViewCANTx_Model * pModel,
        { pModel->bBusOff = bBusOff; },
        true);
}

void CANTransceiver_ViewCANTx_SetEventCallback(
    CANTransceiver_ViewCANTx* pViewCANTx,
    CANTransceiver_ViewCANTx_Callback pCallback,
    void* pCallbackContext) {
    ASSERT(pViewCANTx != NULL);
    ASSERT(pCallback != NULL);

    pViewCANTx->pCallback = pCallback;
    pViewCANTx->pCallbackContext = pCallbackContext;
}
