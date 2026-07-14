// Header for can_transceiver_view_canrx.c.

#ifndef CAN_TRANSCEIVER_VIEW_CANRX_H
#define CAN_TRANSCEIVER_VIEW_CANRX_H

// ==== INCLUDES ====

// GUI
#include <gui/view.h>

// ==== TYPEDEFS ====

typedef enum {
    CANTransceiver_ViewCANRx_EventUnknown,
    CANTransceiver_ViewCANRx_EventPrevMsg,
    CANTransceiver_ViewCANRx_EventNextMsg,
    CANTransceiver_ViewCANRx_EventSaveHistory,
    CANTransceiver_ViewCANRx_EventClearHistory,
} CANTransceiver_ViewCANRx_Event;

typedef void (
    *CANTransceiver_ViewCANRx_Callback)(CANTransceiver_ViewCANRx_Event event, void* pContext);

typedef struct {
    View* pView;
    CANTransceiver_ViewCANRx_Callback pCallback;
    void* pCallbackContext;
} CANTransceiver_ViewCANRx;

// ==== CONSTANTS ====

#define CANRX_MAX_MSGS_PER_SCREEN 3

// ==== PROTOTYPES ====

CANTransceiver_ViewCANRx* CANTransceiver_ViewCANRx_Alloc();
void CANTransceiver_ViewCANRx_Free(CANTransceiver_ViewCANRx* pViewCANRx);
View* CANTransceiver_ViewCANRx_GetView(CANTransceiver_ViewCANRx* pViewCANRx);

void CANTransceiver_ViewCANRx_SetHistoryNumElements(
    CANTransceiver_ViewCANRx* pViewCANRx,
    uint16_t u16HistoryNumElements);
void CANTransceiver_ViewCANRx_SetHistoryIndex(
    CANTransceiver_ViewCANRx* pViewCANRx,
    uint16_t u16HistoryIndex);
void CANTransceiver_ViewCANRx_SetHistoryListOffset(
    CANTransceiver_ViewCANRx* pViewCANRx,
    uint16_t u16HistoryListOffset);
void CANTransceiver_ViewCANRx_SetRxHistoryPtr(
    CANTransceiver_ViewCANRx* pViewCANRx,
    CANTransceiver_RxHistory* pRxHistory);
void CANTransceiver_ViewCANRx_SetREC(CANTransceiver_ViewCANRx* pViewCANRx, uint8_t u8REC);
void CANTransceiver_ViewCANRx_RefreshUI(CANTransceiver_ViewCANRx* pViewCANRx);
void CANTransceiver_ViewCANRx_SetEventCallback(
    CANTransceiver_ViewCANRx* pViewCANRx,
    CANTransceiver_ViewCANRx_Callback pCallback,
    void* pCallbackContext);

#endif // CAN_TRANSCEIVER_VIEW_CANRX_H
