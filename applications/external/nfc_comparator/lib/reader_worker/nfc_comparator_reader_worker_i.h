#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>

#include "nfc_comparator_reader_worker.h"

#include "../compare_worker/nfc_comparator_compare_worker.h"

/** Holds all state for the NFC Comparator Reader Worker */
typedef struct NfcComparatorReaderWorker {
   Nfc* nfc;
   FuriThread* thread;
   NfcProtocol* protocol;
   NfcComparatorReaderWorkerState state;
   NfcDevice* loaded_nfc_card;
   NfcDevice* scanned_nfc_card;
   NfcPoller* nfc_poller;
   NfcScanner* nfc_scanner;
   NfcComparatorCompareWorker* compare_worker;
} NfcComparatorReaderWorker;
