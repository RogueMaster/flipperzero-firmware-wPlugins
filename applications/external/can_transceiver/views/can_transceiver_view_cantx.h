// Header for can_transceiver_view_cantx.c.

#ifndef CAN_TRANSCEIVER_VIEW_CANTX_H
#define CAN_TRANSCEIVER_VIEW_CANTX_H

// ==== INCLUDES ====

// GUI
#include <gui/view.h>

// ==== CONSTANTS ====

#define CANTransceiver_ViewCANTx_STATUS_NONE         (uint8_t)0x00
#define CANTransceiver_ViewCANTx_STATUS_RESETTING    (uint8_t)0x01
#define CANTransceiver_ViewCANTx_STATUS_ERRORACTIVE  (uint8_t)0x02
#define CANTransceiver_ViewCANTx_STATUS_ERRORPASSIVE (uint8_t)0x03
#define CANTransceiver_ViewCANTx_STATUS_BUSOFF       (uint8_t)0x04

// ==== TYPEDEFS ====

typedef enum {
    CANTransceiver_ViewCANTx_EventUnknown,
    CANTransceiver_ViewCANTx_EventEditFrameID,
    CANTransceiver_ViewCANTx_EventEditFrameData,
    CANTransceiver_ViewCANTx_EventResetMCP2515,
    CANTransceiver_ViewCANTx_EventSaveMessage,
    CANTransceiver_ViewCANTx_EventSendMessage,
} CANTransceiver_ViewCANTx_Event;

typedef void (
    *CANTransceiver_ViewCANTx_Callback)(CANTransceiver_ViewCANTx_Event event, void* pContext);

typedef struct {
    View* pView;
    CANTransceiver_ViewCANTx_Callback pCallback;
    void* pCallbackContext;
} CANTransceiver_ViewCANTx;

// ==== PROTOTYPES ====

CANTransceiver_ViewCANTx* CANTransceiver_ViewCANTx_Alloc();
void CANTransceiver_ViewCANTx_Free(CANTransceiver_ViewCANTx* pViewCANTx);
View* CANTransceiver_ViewCANTx_GetView(CANTransceiver_ViewCANTx* pViewCANTx);

void CANTransceiver_ViewCANTx_SetFrameID(
    CANTransceiver_ViewCANTx* pViewCANTx,
    uint32_t u32FrameID,
    uint8_t u8FrameIDLen);
void CANTransceiver_ViewCANTx_SetFrameData(
    CANTransceiver_ViewCANTx* pViewCANTx,
    uint8_t* u8pFrameData,
    uint8_t u8FrameDataLen);
void CANTransceiver_ViewCANTx_SetTEC(CANTransceiver_ViewCANTx* pViewCANTx, uint8_t u8TEC);
void CANTransceiver_ViewCANTx_SetBusOff(CANTransceiver_ViewCANTx* pViewCANTx, uint8_t bBusOff);
void CANTransceiver_ViewCANTx_SetEventCallback(
    CANTransceiver_ViewCANTx* pViewCANTx,
    CANTransceiver_ViewCANTx_Callback pCallback,
    void* pContext);

#endif // CAN_TRANSCEIVER_VIEW_CANTX_H
