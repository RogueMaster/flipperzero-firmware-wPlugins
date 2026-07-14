// Implementation of message history for received CAN messages.

// ==== INCLUDES ====

// Furi
#include <furi.h>

// Mlib
#include <m-array.h>

// App-specific
#include "can_transceiver.h"
#include "can_transceiver_rxhistory.h"

// ==== TYPEDEFS ====

// See mlib documentation for how this works. It's a dynamic array of elements.
// Can't allocate it all on the spot even if there's enough free heap space, the risk of heap fragmentation means it can fail.
ARRAY_DEF(CANTransceiver_RxHistory_Entry_Array, CANTransceiver_RxHistory_Entry, M_POD_OPLIST);

// TODO: Do I need this?
#define M_OPL_CANTransceiver_RxHistory_Entry_Array_t() \
    ARRAY_OPLIST(CANTransceiver_RxHistory_Entry_Array, M_POD_OPLIST)

struct CANTransceiver_RxHistory {
    uint16_t u16NumMessages;
    CANTransceiver_RxHistory_Entry_Array_t pHistoryArray;
};

// ==== PUBLIC FUNCTIONS ====

CANTransceiver_RxHistory* CANTransceiver_RxHistory_Alloc() {
    CANTransceiver_RxHistory* pRxHistory;

    pRxHistory = malloc(sizeof(CANTransceiver_RxHistory));
    pRxHistory->u16NumMessages = 0;
    CANTransceiver_RxHistory_Entry_Array_init(pRxHistory->pHistoryArray);

    return pRxHistory;
}

void CANTransceiver_RxHistory_Free(CANTransceiver_RxHistory* pRxHistory) {
    ASSERT(pRxHistory != NULL);

    for
        M_EACH(pCurEntry, pRxHistory->pHistoryArray, CANTransceiver_RxHistory_Entry_Array_t) {
            memset(pCurEntry, 0, sizeof(CANTransceiver_RxHistory_Entry));
        }
    CANTransceiver_RxHistory_Entry_Array_clear(pRxHistory->pHistoryArray);
    free(pRxHistory);
}

void CANTransceiver_RxHistory_Reset(CANTransceiver_RxHistory* pRxHistory) {
    ASSERT(pRxHistory != NULL);

    // Empties the array but doesn't delete it, for it's being reused.
    // FIXME: I think there's a memory leak here. Calling this function doesn't free much of the used memory.
    // Maybe I'm missing something.
    for
        M_EACH(pCurEntry, pRxHistory->pHistoryArray, CANTransceiver_RxHistory_Entry_Array_t) {
            memset(pCurEntry, 0, sizeof(CANTransceiver_RxHistory_Entry));
        }
    CANTransceiver_RxHistory_Entry_Array_reset(pRxHistory->pHistoryArray);
    pRxHistory->u16NumMessages = 0;
}

CANTransceiver_RxHistory_Entry*
    CANTransceiver_RxHistory_GetEntry(CANTransceiver_RxHistory* pRxHistory, uint16_t u16Index) {
    CANTransceiver_RxHistory_Entry* pEntry;

    ASSERT(pRxHistory != NULL);
    ASSERT(
        u16Index <
        pRxHistory->u16NumMessages); // Something's wrong if we're trying an out-of-bounds access.

    pEntry = CANTransceiver_RxHistory_Entry_Array_get(pRxHistory->pHistoryArray, u16Index);

    return pEntry;
}

bool CANTransceiver_RxHistory_AddEntry(
    CANTransceiver_RxHistory* pRxHistory,
    CANTransceiver_RxHistory_Entry* pEntryToAdd) {
    CANTransceiver_RxHistory_Entry* pNewEntry;

    ASSERT(pRxHistory != NULL);
    ASSERT(pEntryToAdd != NULL);

    // Don't add this if there's no space left.
    if(memmgr_get_free_heap() < u16RXHISTORY_MIN_FREE_HEAP ||
       pRxHistory->u16NumMessages >= u16RXHISTORY_MAX_NUM_MSG)
        return false;

    // Add a new entry, then copy the data at pEntryToAdd to it.
    // It's assumed pEntryToAdd is statically allocated and all the data inside is already set, timestamp included.
    pNewEntry = CANTransceiver_RxHistory_Entry_Array_push_raw(pRxHistory->pHistoryArray);
    memcpy(pNewEntry, pEntryToAdd, sizeof(CANTransceiver_RxHistory_Entry));
    pRxHistory->u16NumMessages++;

    return true;
}
