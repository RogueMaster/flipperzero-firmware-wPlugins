// Header for can_transceiver_rxhistory.c.

#ifndef CAN_TRANSCEIVER_RXHISTORY_H
#define CAN_TRANSCEIVER_RXHISTORY_H

// ==== CONSTANTS ====

// If it's at most 20 KiB (20480 bytes) we want to use for messages, then that's enough to store 20480 / 17 = 1204 of them.
// The array structure is probably a linked list, adding some memory. Let's limit it to 1000.
#define u16RXHISTORY_MAX_NUM_MSG (uint16_t)1000

// We don't want the free heap space falling under this; no telling what could happen. Should keep at least 20 KiB.
#define u16RXHISTORY_MIN_FREE_HEAP (uint16_t)20480

// ==== TYPEDEFS ====

typedef struct {
    uint32_t u32FrameID; // NOTE: All ones means this is an error frame.
    uint8_t u8pFrameData[8];
    uint32_t u32Timestamp; // For logging + analysis purposes.
    uint8_t u4FrameDataLen       : 4;
    uint8_t bExtendedIDFrame     : 1;
    uint8_t bRemoteTransferFrame : 1;
    uint8_t reserved             : 2;
} CANTransceiver_RxHistory_Entry; // 17 bytes long.

typedef struct CANTransceiver_RxHistory
    CANTransceiver_RxHistory; // See can_transceiver_rxhistory.c for definition.

// ==== FUNCTION PROTOTYPES ====

CANTransceiver_RxHistory* CANTransceiver_RxHistory_Alloc();
void CANTransceiver_RxHistory_Free(CANTransceiver_RxHistory* pRxHistory);
void CANTransceiver_RxHistory_Reset(CANTransceiver_RxHistory* pRxHistory);

CANTransceiver_RxHistory_Entry*
    CANTransceiver_RxHistory_GetEntry(CANTransceiver_RxHistory* pRxHistory, uint16_t u16Index);
bool CANTransceiver_RxHistory_AddEntry(
    CANTransceiver_RxHistory* pRxHistory,
    CANTransceiver_RxHistory_Entry* pEntryToAdd);

#endif // CAN_TRANSCEIVER_RXHISTORY_H
