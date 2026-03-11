#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorFinderWorker NfcComparatorFinderWorker;
typedef struct NfcComparatorCompareWorker NfcComparatorCompareWorker;
typedef struct NfcDevice NfcDevice;
typedef struct FuriString FuriString;

/**
 * @enum NfcComparatorFinderWorkerState
 * @brief Possible states for the NFC Comparator Reader Worker.
 */
typedef enum {
   NfcComparatorFinderWorkerState_Scanning, /**< Worker is scanning for NFC cards */
   NfcComparatorFinderWorkerState_Polling, /**< Worker is polling for NFC cards */
   NfcComparatorFinderWorkerState_Finding, /**< Worker is searching for a specific NFC card */
   NfcComparatorFinderWorkerState_Stopped /**< Worker is stopped */
} NfcComparatorFinderWorkerState;

/**
 * @struct NfcComparatorFinderWorkerSettings
 * @brief Holds settings for the NFC Comparator Finder Worker.
 */
typedef struct {
   bool recursive; /**< Whether to search recursively */
} NfcComparatorFinderWorkerSettings;

/**
 * @brief Allocate and initialize a new NfcComparatorFinderWorker.
 * @param compare_worker Pointer to a NfcComparatorCompareWorker structure for comparison results.
 * @param settings Pointer to a NfcComparatorFinderWorkerSettings structure for configuration.
 * @return Pointer to the allocated NfcComparatorFinderWorker, or NULL on failure.
 */
NfcComparatorFinderWorker* nfc_comparator_finder_worker_alloc(
   NfcComparatorCompareWorker* compare_worker,
   NfcComparatorFinderWorkerSettings* settings);

/**
 * @brief Free all resources associated with a NfcComparatorFinderWorker.
 * @param worker Pointer to the worker to free.
 */
void nfc_comparator_finder_worker_free(NfcComparatorFinderWorker* worker);

/**
 * @brief Stop the worker thread and set its state to Stopped.
 * @param worker Pointer to the worker to stop.
 */
void nfc_comparator_finder_worker_stop(NfcComparatorFinderWorker* worker);

/**
 * @brief Start the worker thread if it is currently stopped.
 * @param worker Pointer to the worker to start.
 */
void nfc_comparator_finder_worker_start(NfcComparatorFinderWorker* worker);

/**
 * @brief Get the current state of the worker.
 * @param worker Pointer to the worker.
 * @return The current NfcComparatorFinderWorkerState.
 */
NfcComparatorFinderWorkerState*
   nfc_comparator_finder_worker_get_state(NfcComparatorFinderWorker* worker);

/**
 * @brief Compare a given NFC card against all stored NFC cards.
 * Iterates through stored NFC card files and compares each to the provided card.
 * Updates the compare_worker structure with the results.
 * @param compare_worker Pointer to the comparison results structure.
 * @param nfc_card_1 Pointer to the NFC card to compare.
 * @param check_data Whether to compare full NFC data in addition to UID/protocol.
 * @param settings Pointer to the finder worker settings.
 * @param nfc_card_path Optional: path to skip during comparison (can be NULL).
 */
void nfc_comparator_finder_worker_compare_cards(
   NfcComparatorCompareWorker* compare_worker,
   NfcDevice* nfc_card_1,
   NfcComparatorFinderWorkerSettings* settings,
   FuriString* nfc_card_path);

#ifdef __cplusplus
}
#endif
