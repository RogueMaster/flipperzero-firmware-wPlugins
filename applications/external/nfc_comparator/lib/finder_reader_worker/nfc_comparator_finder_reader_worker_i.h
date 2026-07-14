#pragma once

#include <furi.h>
#include <nfc/nfc.h>

#include <path.h>
#include <storage/storage.h>
#include <dir_walk.h>

#include <nfc/nfc_scanner.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>

#include "nfc_comparator_finder_reader_worker.h"

#include "../compare_worker/nfc_comparator_compare_worker.h"
#include "../finder_searcher_worker/nfc_comparator_finder_searcher_worker.h"

/**
 * @struct NfcComparatorFinderReaderWorker
 * @brief Internal structure holding all state for the NFC Comparator Finder Worker.
 */
typedef struct NfcComparatorFinderReaderWorker {
   Nfc* nfc; /**< Pointer to the NFC context */
   FuriThread* thread; /**< Worker thread */
   NfcProtocol* protocol; /**< Protocol in use */
   NfcComparatorFinderReaderWorkerState state; /**< Current worker state */
   NfcDevice* loaded_nfc_card; /**< NFC card loaded from storage */
   NfcDevice* scanned_nfc_card; /**< NFC card scanned from reader */
   NfcPoller* nfc_poller; /**< NFC poller instance */
   NfcComparatorCompareWorker* compare_worker; /**< Comparison results structure */
   NfcComparatorFinderSearcherWorker* searcher_worker; /**< The searcher for the filesystem */
   NfcComparatorFinderSearcherWorkerSettings* settings; /**< Worker settings */
} NfcComparatorFinderReaderWorker;
