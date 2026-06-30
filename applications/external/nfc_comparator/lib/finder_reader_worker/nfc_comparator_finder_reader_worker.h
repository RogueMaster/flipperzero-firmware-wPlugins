#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorFinderReaderWorker NfcComparatorFinderReaderWorker;
typedef struct NfcComparatorCompareWorker NfcComparatorCompareWorker;
typedef struct NfcComparatorFinderSearcherWorkerSettings NfcComparatorFinderSearcherWorkerSettings;
typedef struct NfcDevice NfcDevice;
typedef struct FuriString FuriString;

/**
 * @enum NfcComparatorFinderReaderWorkerState
 * @brief Possible states for the NFC Comparator Reader Worker.
 */
typedef enum {
   NfcComparatorFinderReaderWorkerState_Scanning, /**< Worker is scanning for NFC cards */
   NfcComparatorFinderReaderWorkerState_Polling, /**< Worker is polling for NFC cards */
   NfcComparatorFinderReaderWorkerState_Finding, /**< Worker is searching for a specific NFC card */
   NfcComparatorFinderReaderWorkerState_Stopped /**< Worker is stopped */
} NfcComparatorFinderReaderWorkerState;

/**
 * @brief Allocate and initialize a new NfcComparatorFinderReaderWorker.
 * @param compare_worker Pointer to a NfcComparatorCompareWorker structure for comparison results.
 * @param settings Pointer to a NfcComparatorFinderReaderWorkerSettings structure for configuration.
 * @return Pointer to the allocated NfcComparatorFinderReaderWorker, or NULL on failure.
 */
NfcComparatorFinderReaderWorker* nfc_comparator_finder_reader_worker_alloc(
   NfcComparatorCompareWorker* compare_worker,
   NfcComparatorFinderSearcherWorkerSettings* settings);

/**
 * @brief Free all resources associated with a NfcComparatorFinderReaderWorker.
 * @param worker Pointer to the worker to free.
 */
void nfc_comparator_finder_reader_worker_free(NfcComparatorFinderReaderWorker* worker);

/**
 * @brief Stop the worker thread and set its state to Stopped.
 * @param worker Pointer to the worker to stop.
 */
void nfc_comparator_finder_reader_worker_stop(NfcComparatorFinderReaderWorker* worker);

/**
 * @brief Start the worker thread if it is currently stopped.
 * @param worker Pointer to the worker to start.
 */
void nfc_comparator_finder_reader_worker_start(NfcComparatorFinderReaderWorker* worker);

/**
 * @brief Get the current state of the worker.
 * @param worker Pointer to the worker.
 * @return The current NfcComparatorFinderReaderWorkerState.
 */
NfcComparatorFinderReaderWorkerState*
   nfc_comparator_finder_reader_worker_get_state(NfcComparatorFinderReaderWorker* worker);

#ifdef __cplusplus
}
#endif
