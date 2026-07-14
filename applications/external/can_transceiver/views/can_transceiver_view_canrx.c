// CAN message reception and history view.

// ==== INCLUDES ====

// Libc
#include <stdint.h>

// GUI
#include <gui/elements.h>

// App-specific
#include "can_transceiver.h"
#include "can_transceiver_rxhistory.h"
#include "can_transceiver_view_canrx.h"

// ==== TYPEDEFS ====

typedef struct {
    uint16_t u16HistoryNumElements;
    uint16_t u16HistoryIndex;
    uint16_t u16HistoryListOffset;
    CANTransceiver_RxHistory* pRxHistory;
    uint8_t u8REC;
} CANTransceiver_ViewCANRx_Model;

// ==== CONSTANTS ====

#define CANRX_ENTRY_HEIGHT 17

// ==== STATIC FUNCTIONS ====

static void CANTransceiver_ViewCANRx_DrawCallback(Canvas* pCanvas, void* pModelVoid) {
    CANTransceiver_ViewCANRx_Model* pModel = pModelVoid;
    CANTransceiver_RxHistory_Entry* pEntry;
    uint16_t i;
    uint16_t u16RelativeIndex;
    bool bScrollbar;
    char i8pTempBuf[32];
    char i8pTempBuf2[3];
    uint8_t j;

    ASSERT(pCanvas != NULL);
    ASSERT(pModelVoid != NULL);

    canvas_clear(pCanvas);
    canvas_set_color(pCanvas, ColorBlack);
    canvas_set_font(pCanvas, FontKeyboard); // 7x7 font.
    // REMOVED: No space for these.
#if 0
	elements_button_right(pCanvas, "Save");
	elements_button_left(pCanvas, "Clear");
#endif

    // Scrollbar, if there are too many items to show.
    bScrollbar = pModel->u16HistoryNumElements > CANRX_MAX_MSGS_PER_SCREEN;
    if(bScrollbar) {
        elements_scrollbar_pos(
            pCanvas, 128, 0, 49, pModel->u16HistoryIndex, pModel->u16HistoryNumElements);
    }

    // Draw each visible message, if there are any.
    for(i = 0; i < MIN(pModel->u16HistoryNumElements, CANRX_MAX_MSGS_PER_SCREEN); i++) {
        u16RelativeIndex =
            CLAMP(i + pModel->u16HistoryListOffset, pModel->u16HistoryNumElements, 0);
        pEntry = CANTransceiver_RxHistory_GetEntry(pModel->pRxHistory, u16RelativeIndex);
        if(pModel->u16HistoryIndex == u16RelativeIndex) {
            canvas_set_color(pCanvas, ColorBlack);
            canvas_draw_box(
                pCanvas,
                0,
                0 + (i * CANRX_ENTRY_HEIGHT),
                bScrollbar ? 122 : 127,
                CANRX_ENTRY_HEIGHT);
            canvas_set_color(pCanvas, ColorWhite);
        } else {
            canvas_set_color(pCanvas, ColorBlack);
        }

        // NOTE: Stuff below is copy-pasted and changed from can_transceiver_view_cantx.c.

        // Frame ID
        memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
        if(pEntry->bExtendedIDFrame) {
            snprintf(i8pTempBuf, 21, "ID: 0x%08lx", pEntry->u32FrameID);
        } else {
            snprintf(i8pTempBuf, 21, "ID: 0x%04lx", pEntry->u32FrameID);
        }
        i8pTempBuf[20] = 0;
        canvas_draw_str(pCanvas, 1, 8 + (i * CANRX_ENTRY_HEIGHT), i8pTempBuf);

        // Frame data
        memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
        memcpy(i8pTempBuf, "D: ", sizeof("D: ") - 1);
        for(j = 0; j < pEntry->u4FrameDataLen; j++) {
            snprintf(i8pTempBuf2, 3, "%02hhx", pEntry->u8pFrameData[j]);
            i8pTempBuf2[2] = 0;
            memcpy(i8pTempBuf + 3 + (j * 2), i8pTempBuf2, 3);
        }
        i8pTempBuf[20] = 0;
        canvas_draw_str(pCanvas, 1, 16 + (i * CANRX_ENTRY_HEIGHT), i8pTempBuf);

        // Flags
        memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
        i8pTempBuf[0] = 'F';
        i8pTempBuf[1] = ':';
        if(pEntry->bExtendedIDFrame)
            i8pTempBuf[2] = 'X';
        else
            i8pTempBuf[2] = '-';
        if(pEntry->bRemoteTransferFrame)
            i8pTempBuf[3] = 'R';
        else
            i8pTempBuf[3] = '-';
        canvas_draw_str(pCanvas, 96, 8 + (i * CANRX_ENTRY_HEIGHT), i8pTempBuf);
    }
    canvas_set_color(pCanvas, ColorBlack);
    // If there were no messages, print a filler message.
    if(pModel->u16HistoryNumElements == 0) {
        canvas_draw_str(pCanvas, 0, 8, "Awaiting messages...");
    }

    // Draw the REC.
    memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
    snprintf(i8pTempBuf, 10, "REC: %hhd", pModel->u8REC);
    canvas_draw_str(pCanvas, 0, 64, i8pTempBuf);

    // Draw the number of messages received.
    if(pModel->u16HistoryNumElements >= u16RXHISTORY_MAX_NUM_MSG) {
        canvas_draw_str(pCanvas, 80, 64, "No more!");
    } else {
        memset(i8pTempBuf, 0, sizeof(i8pTempBuf));
        snprintf(
            i8pTempBuf, 10, "%04hd/%hd", pModel->u16HistoryNumElements, u16RXHISTORY_MAX_NUM_MSG);
        canvas_draw_str(pCanvas, 72, 64, i8pTempBuf);
    }
}

static bool CANTransceiver_ViewCANRx_InputCallback(InputEvent* pInputEvent, void* pContext) {
    CANTransceiver_ViewCANRx* pViewCANRx;
    CANTransceiver_ViewCANRx_Event eventToForward;
    bool bSuccess;

    ASSERT(pInputEvent != NULL);
    ASSERT(pContext != NULL);

    pViewCANRx = pContext;
    eventToForward = CANTransceiver_ViewCANRx_EventUnknown;
    bSuccess = false;
    if(pInputEvent->type == InputTypeShort) {
        switch(pInputEvent->key) {
        case InputKeyUp:
            eventToForward = CANTransceiver_ViewCANRx_EventPrevMsg;
            break;
        case InputKeyDown:
            eventToForward = CANTransceiver_ViewCANRx_EventNextMsg;
            break;
        case InputKeyLeft:
            eventToForward = CANTransceiver_ViewCANRx_EventClearHistory;
            break;
        case InputKeyRight:
            eventToForward = CANTransceiver_ViewCANRx_EventSaveHistory;
            break;
        default:
            break;
        }
    }

    ASSERT(pViewCANRx->pCallback != NULL);
    if(eventToForward != CANTransceiver_ViewCANRx_EventUnknown) {
        bSuccess = true;
        pViewCANRx->pCallback(eventToForward, pViewCANRx->pCallbackContext);
    }

    return bSuccess;
}

static void CANTransceiver_ViewCANRx_EnterCallback(void* pContext) {
    CANTransceiver_ViewCANRx* pViewCANRx = pContext;

    ASSERT(pContext != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        true);
}

static void CANTransceiver_ViewCANRx_ExitCallback(void* pContext) {
    CANTransceiver_ViewCANRx* pViewCANRx = pContext;

    ASSERT(pContext != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        false);
}

// ==== PUBLIC FUNCTIONS ====

CANTransceiver_ViewCANRx* CANTransceiver_ViewCANRx_Alloc() {
    CANTransceiver_ViewCANRx* pViewCANRx;

    pViewCANRx = malloc(sizeof(CANTransceiver_ViewCANRx));
    pViewCANRx->pView = view_alloc();
    view_set_context(pViewCANRx->pView, pViewCANRx);
    view_allocate_model(
        pViewCANRx->pView, ViewModelTypeLocking, sizeof(CANTransceiver_ViewCANRx_Model));
    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        false);

    view_set_draw_callback(pViewCANRx->pView, CANTransceiver_ViewCANRx_DrawCallback);
    view_set_enter_callback(pViewCANRx->pView, CANTransceiver_ViewCANRx_EnterCallback);
    view_set_exit_callback(pViewCANRx->pView, CANTransceiver_ViewCANRx_ExitCallback);
    view_set_input_callback(pViewCANRx->pView, CANTransceiver_ViewCANRx_InputCallback);

    return pViewCANRx;
}

void CANTransceiver_ViewCANRx_Free(CANTransceiver_ViewCANRx* pViewCANRx) {
    ASSERT(pViewCANRx != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        {
            (void)pModel; // Nothing to do here.
        },
        false);

    view_free(pViewCANRx->pView);
    free(pViewCANRx);
}

View* CANTransceiver_ViewCANRx_GetView(CANTransceiver_ViewCANRx* pViewCANRx) {
    ASSERT(pViewCANRx != NULL);

    return pViewCANRx->pView;
}

void CANTransceiver_ViewCANRx_SetHistoryNumElements(
    CANTransceiver_ViewCANRx* pViewCANRx,
    uint16_t u16HistoryNumElements) {
    ASSERT(pViewCANRx != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        { pModel->u16HistoryNumElements = u16HistoryNumElements; },
        false);
}

void CANTransceiver_ViewCANRx_SetHistoryIndex(
    CANTransceiver_ViewCANRx* pViewCANRx,
    uint16_t u16HistoryIndex) {
    ASSERT(pViewCANRx != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        { pModel->u16HistoryIndex = u16HistoryIndex; },
        false);
}

void CANTransceiver_ViewCANRx_SetHistoryListOffset(
    CANTransceiver_ViewCANRx* pViewCANRx,
    uint16_t u16HistoryListOffset) {
    ASSERT(pViewCANRx != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        { pModel->u16HistoryListOffset = u16HistoryListOffset; },
        false);
}

void CANTransceiver_ViewCANRx_SetRxHistoryPtr(
    CANTransceiver_ViewCANRx* pViewCANRx,
    CANTransceiver_RxHistory* pRxHistory) {
    ASSERT(pViewCANRx != NULL);
    ASSERT(pRxHistory != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        { pModel->pRxHistory = pRxHistory; },
        false);
}

void CANTransceiver_ViewCANRx_SetREC(CANTransceiver_ViewCANRx* pViewCANRx, uint8_t u8REC) {
    ASSERT(pViewCANRx != NULL);

    with_view_model(
        pViewCANRx->pView,
        CANTransceiver_ViewCANRx_Model * pModel,
        { pModel->u8REC = u8REC; },
        false);
}

void CANTransceiver_ViewCANRx_RefreshUI(CANTransceiver_ViewCANRx* pViewCANRx) {
    ASSERT(pViewCANRx != NULL);

    with_view_model(
        pViewCANRx->pView, CANTransceiver_ViewCANRx_Model * pModel, { (void)pModel; }, true);
}

void CANTransceiver_ViewCANRx_SetEventCallback(
    CANTransceiver_ViewCANRx* pViewCANRx,
    CANTransceiver_ViewCANRx_Callback pCallback,
    void* pCallbackContext) {
    ASSERT(pViewCANRx != NULL);
    ASSERT(pCallback != NULL);

    pViewCANRx->pCallback = pCallback;
    pViewCANRx->pCallbackContext = pCallbackContext;
}
