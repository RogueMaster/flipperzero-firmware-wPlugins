#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorCompareWorker NfcComparatorCompareWorker;
typedef struct NfcComparatorCompareReaderWorker NfcComparatorCompareReaderWorker;

/** Possible states for the NFC Comparator Reader Worker */
typedef enum {
   NfcComparatorCompareReaderWorkerState_Scanning,
   NfcComparatorCompareReaderWorkerState_Polling,
   NfcComparatorCompareReaderWorkerState_Comparing,
   NfcComparatorCompareReaderWorkerState_Stopped
} NfcComparatorCompareReaderWorkerState;

/** Allocates and initializes a new NFC Comparator Reader Worker */
NfcComparatorCompareReaderWorker*
   nfc_comparator_compare_reader_worker_alloc(NfcComparatorCompareWorker* compare_worker);

/** Frees the resources used by the worker */
void nfc_comparator_compare_reader_worker_free(NfcComparatorCompareReaderWorker* worker);

/** Stops the worker thread */
void nfc_comparator_compare_reader_worker_stop(NfcComparatorCompareReaderWorker* worker);

/** Starts the worker thread */
void nfc_comparator_compare_reader_worker_start(NfcComparatorCompareReaderWorker* worker);

/** Loads an NFC card from the given path */
bool nfc_comparator_compare_reader_worker_set_loaded_nfc_card(
   NfcComparatorCompareReaderWorker* worker,
   const char* path_to_nfc_card);

/** Gets the current state of the worker */
const NfcComparatorCompareReaderWorkerState*
   nfc_comparator_compare_reader_worker_get_state(NfcComparatorCompareReaderWorker* worker);

#ifdef __cplusplus
}
#endif
