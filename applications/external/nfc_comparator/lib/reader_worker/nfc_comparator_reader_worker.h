#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorCompareWorker NfcComparatorCompareWorker;
typedef struct NfcComparatorReaderWorker NfcComparatorReaderWorker;

/** Possible states for the NFC Comparator Reader Worker */
typedef enum {
   NfcComparatorReaderWorkerState_Scanning,
   NfcComparatorReaderWorkerState_Polling,
   NfcComparatorReaderWorkerState_Comparing,
   NfcComparatorReaderWorkerState_Stopped
} NfcComparatorReaderWorkerState;

/** Allocates and initializes a new NFC Comparator Reader Worker */
NfcComparatorReaderWorker*
   nfc_comparator_reader_worker_alloc(NfcComparatorCompareWorker* compare_worker);

/** Frees the resources used by the worker */
void nfc_comparator_reader_worker_free(NfcComparatorReaderWorker* worker);

/** Stops the worker thread */
void nfc_comparator_reader_worker_stop(NfcComparatorReaderWorker* worker);

/** Starts the worker thread */
void nfc_comparator_reader_worker_start(NfcComparatorReaderWorker* worker);

/** Loads an NFC card from the given path */
bool nfc_comparator_reader_worker_set_loaded_nfc_card(
   NfcComparatorReaderWorker* worker,
   const char* path_to_nfc_card);

/** Gets the current state of the worker */
const NfcComparatorReaderWorkerState*
   nfc_comparator_reader_worker_get_state(NfcComparatorReaderWorker* worker);

#ifdef __cplusplus
}
#endif
