#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

typedef enum {
    DndStorageTransferFailed = 0, /* No new durable transfer intent was published. */
    DndStorageTransferComplete,
    DndStorageTransferPending, /* Intent is durable or uncertain: discard selection, never retry it. */
    DndStorageTransferRecovered, /* An earlier intent was recovered; no new transfer was started. */
} DndStorageTransferResult;

/* Complete a committed pair before any inventory read or write. A corrupt or
   unreadable journal fails closed and preserves all artifacts. recovered is set
   when a committed journal was found, including when recovery cannot finish. */
bool dnd_inventory_transaction_recover(Storage* storage, uint32_t profile, bool* recovered);

/* Both snapshot inputs must already be synced and closed. After the journal
   commit point, failures return Pending, never Failed. This helper retains the
   snapshot inputs and publishes only verified private staging copies. */
DndStorageTransferResult dnd_inventory_transaction_publish(
    Storage* storage,
    uint32_t profile,
    const char* source_snapshot,
    const char* source_live,
    const char* destination_snapshot,
    const char* destination_live);
