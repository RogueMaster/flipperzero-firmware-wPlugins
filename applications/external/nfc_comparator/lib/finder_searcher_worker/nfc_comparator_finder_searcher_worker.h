#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorFinderSearcherWorker NfcComparatorFinderSearcherWorker;
typedef struct NfcComparatorCompareWorker NfcComparatorCompareWorker;
typedef struct NfcDevice NfcDevice;
typedef struct FuriString FuriString;
typedef struct FuriThread FuriThread;
typedef struct DirWalk DirWalk;

/** Possible states for the NFC Comparator Searcher Worker */
typedef enum {
   NfcComparatorFinderSearcherWorkerState_Stopped,
   NfcComparatorFinderSearcherWorkerState_Searching,
   NfcComparatorFinderSearcherWorkerState_Idle
} NfcComparatorFinderSearcherWorkerState;

/**
 * @struct NfcComparatorFinderSearcherWorkerSettings
 * @brief Holds settings for the NFC Comparator Searcher Worker.
 */
typedef struct NfcComparatorFinderSearcherWorkerSettings {
   bool recursive; /**< Whether to search recursively */
} NfcComparatorFinderSearcherWorkerSettings;

typedef struct NfcComparatorFinderSearcherWorker {
   FuriThread* thread;
   NfcComparatorFinderSearcherWorkerSettings* settings;
   NfcComparatorFinderSearcherWorkerState state;
   NfcComparatorCompareWorker* compare_worker;
   DirWalk* dir_walk;
   NfcDevice* nfc_card_1;
   FuriString* nfc_card_path;
} NfcComparatorFinderSearcherWorker;

NfcComparatorFinderSearcherWorker* nfc_comparator_finder_searcher_worker_alloc(
   NfcComparatorCompareWorker* compare_worker,
   NfcComparatorFinderSearcherWorkerSettings* settings,
   NfcDevice* nfc_card_1,
   FuriString* nfc_card_path);

void nfc_comparator_finder_searcher_worker_free(NfcComparatorFinderSearcherWorker* worker);

void nfc_comparator_finder_searcher_worker_stop(NfcComparatorFinderSearcherWorker* worker);

void nfc_comparator_finder_searcher_worker_start(NfcComparatorFinderSearcherWorker* worker);

NfcComparatorFinderSearcherWorkerState*
   nfc_comparator_finder_searcher_worker_get_state(NfcComparatorFinderSearcherWorker* worker);

#ifdef __cplusplus
}
#endif
