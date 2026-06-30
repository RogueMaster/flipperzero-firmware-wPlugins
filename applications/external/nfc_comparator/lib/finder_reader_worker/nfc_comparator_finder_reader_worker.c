#include "nfc_comparator_finder_reader_worker_i.h"

static void
   nfc_comparator_finder_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   furi_assert(context);
   NfcComparatorFinderReaderWorker* nfc_comparator_finder_reader_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      nfc_comparator_finder_reader_worker->protocol = event.data.protocols;
      nfc_comparator_finder_reader_worker->state = NfcComparatorFinderReaderWorkerState_Polling;
      break;
   default:
      break;
   }
}

static NfcCommand
   nfc_comparator_finder_reader_worker_poller_callback(NfcGenericEvent event, void* context) {
   UNUSED(event);
   furi_assert(context);
   NfcComparatorFinderReaderWorker* nfc_comparator_finder_reader_worker = context;
   nfc_comparator_finder_reader_worker->state = NfcComparatorFinderReaderWorkerState_Finding;
   return NfcCommandStop;
}

static int32_t nfc_comparator_finder_reader_worker_task(void* context) {
   NfcComparatorFinderReaderWorker* worker = context;
   furi_assert(worker);
   while(worker->state != NfcComparatorFinderReaderWorkerState_Stopped) {
      switch(worker->state) {
      case NfcComparatorFinderReaderWorkerState_Scanning: {
         NfcScanner* nfc_scanner = nfc_scanner_alloc(worker->nfc);
         nfc_scanner_start(
            nfc_scanner, nfc_comparator_finder_reader_worker_scanner_callback, worker);
         while(worker->state == NfcComparatorFinderReaderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(nfc_scanner);
         break;
      }
      case NfcComparatorFinderReaderWorkerState_Polling: {
         NfcPoller* nfc_poller = nfc_poller_alloc(worker->nfc, worker->protocol[0]);
         nfc_poller_start(nfc_poller, nfc_comparator_finder_reader_worker_poller_callback, worker);
         while(worker->state == NfcComparatorFinderReaderWorkerState_Polling) {
            furi_delay_ms(100);
         }
         nfc_poller_stop(nfc_poller);
         worker->scanned_nfc_card = nfc_device_alloc();
         nfc_device_set_data(
            worker->scanned_nfc_card,
            worker->protocol[0],
            (NfcDeviceData*)nfc_poller_get_data(nfc_poller));
         nfc_poller_free(nfc_poller);
         break;
      }
      case NfcComparatorFinderReaderWorkerState_Finding: {
         worker->searcher_worker = nfc_comparator_finder_searcher_worker_alloc(
            worker->compare_worker, worker->settings, worker->scanned_nfc_card, NULL);
         nfc_comparator_finder_searcher_worker_start(worker->searcher_worker);

         while(worker->state == NfcComparatorFinderReaderWorkerState_Finding) {
            if(worker->searcher_worker->state == NfcComparatorFinderSearcherWorkerState_Stopped) {
               break;
            }
            furi_delay_ms(100);
         }

         if(worker->searcher_worker) {
            nfc_comparator_finder_searcher_worker_stop(worker->searcher_worker);
            nfc_comparator_finder_searcher_worker_free(worker->searcher_worker);
            worker->searcher_worker = NULL;
         }

         worker->state = NfcComparatorFinderReaderWorkerState_Stopped;
         break;
      }
      default:
         break;
      }
   }
   return 0;
}

NfcComparatorFinderReaderWorker* nfc_comparator_finder_reader_worker_alloc(
   NfcComparatorCompareWorker* compare_worker,
   NfcComparatorFinderSearcherWorkerSettings* settings) {
   NfcComparatorFinderReaderWorker* worker = calloc(1, sizeof(NfcComparatorFinderReaderWorker));
   if(!worker) return NULL;
   worker->nfc = nfc_alloc();
   if(!worker->nfc) {
      free(worker);
      return NULL;
   }

   worker->compare_worker = compare_worker;
   worker->settings = settings;

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "NfcComparatorFinderReaderWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 4096);
   furi_thread_set_callback(worker->thread, nfc_comparator_finder_reader_worker_task);

   if(!worker->thread) {
      nfc_free(worker->nfc);
      free(worker);
      return NULL;
   }
   worker->state = NfcComparatorFinderReaderWorkerState_Stopped;
   worker->scanned_nfc_card = NULL;
   worker->loaded_nfc_card = nfc_device_alloc();
   worker->protocol = NULL;

   return worker;
}

void nfc_comparator_finder_reader_worker_free(NfcComparatorFinderReaderWorker* worker) {
   furi_assert(worker);
   worker->state = NfcComparatorFinderReaderWorkerState_Stopped;
   if(worker->searcher_worker) {
      nfc_comparator_finder_searcher_worker_free(worker->searcher_worker);
      worker->searcher_worker = NULL;
   }
   if(worker->loaded_nfc_card) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
   }
   if(worker->scanned_nfc_card) {
      nfc_device_free(worker->scanned_nfc_card);
      worker->scanned_nfc_card = NULL;
   }
   nfc_free(worker->nfc);
   furi_thread_free(worker->thread);
   free(worker);
}

void nfc_comparator_finder_reader_worker_stop(NfcComparatorFinderReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcComparatorFinderReaderWorkerState_Stopped &&
      worker->state != NfcComparatorFinderReaderWorkerState_Finding) {
      worker->state = NfcComparatorFinderReaderWorkerState_Stopped;
      furi_thread_join(worker->thread);
   } else if(worker->state == NfcComparatorFinderReaderWorkerState_Finding && worker->searcher_worker) {
      worker->state = NfcComparatorFinderReaderWorkerState_Stopped;
      nfc_comparator_finder_searcher_worker_stop(worker->searcher_worker);
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_finder_reader_worker_start(NfcComparatorFinderReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorFinderReaderWorkerState_Stopped) {
      worker->state = NfcComparatorFinderReaderWorkerState_Scanning;
      furi_thread_start(worker->thread);
   }
}

NfcComparatorFinderReaderWorkerState*
   nfc_comparator_finder_reader_worker_get_state(NfcComparatorFinderReaderWorker* worker) {
   furi_assert(worker);
   return &worker->state;
}
