#include "nfc_comparator_compare_reader_worker_i.h"

static void
   nfc_comparator_compare_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   furi_assert(context);
   NfcComparatorCompareReaderWorker* nfc_comparator_compare_reader_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      nfc_comparator_compare_reader_worker->protocol = event.data.protocols;
      nfc_comparator_compare_reader_worker->state = NfcComparatorCompareReaderWorkerState_Polling;
      break;
   default:
      break;
   }
}

static NfcCommand
   nfc_comparator_compare_reader_worker_poller_callback(NfcGenericEvent event, void* context) {
   furi_assert(context);
   UNUSED(event);
   NfcComparatorCompareReaderWorker* nfc_comparator_compare_reader_worker = context;
   nfc_comparator_compare_reader_worker->state = NfcComparatorCompareReaderWorkerState_Comparing;
   return NfcCommandStop;
}

static int32_t nfc_comparator_compare_reader_worker_task(void* context) {
   NfcComparatorCompareReaderWorker* worker = context;
   furi_assert(worker);
   while(worker->state != NfcComparatorCompareReaderWorkerState_Stopped) {
      switch(worker->state) {
      case NfcComparatorCompareReaderWorkerState_Scanning: {
         worker->nfc_scanner = nfc_scanner_alloc(worker->nfc);
         nfc_scanner_start(
            worker->nfc_scanner, nfc_comparator_compare_reader_worker_scanner_callback, worker);
         while(worker->state == NfcComparatorCompareReaderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(worker->nfc_scanner);
         nfc_scanner_free(worker->nfc_scanner);
         worker->nfc_scanner = NULL;
         break;
      }
      case NfcComparatorCompareReaderWorkerState_Polling: {
         worker->nfc_poller = nfc_poller_alloc(worker->nfc, worker->protocol[0]);
         nfc_poller_start(
            worker->nfc_poller, nfc_comparator_compare_reader_worker_poller_callback, worker);
         while(worker->state == NfcComparatorCompareReaderWorkerState_Polling) {
            furi_delay_ms(100);
         }
         nfc_poller_stop(worker->nfc_poller);
         worker->scanned_nfc_card = nfc_device_alloc();
         nfc_device_set_data(
            worker->scanned_nfc_card,
            worker->protocol[0],
            (NfcDeviceData*)nfc_poller_get_data(worker->nfc_poller));
         nfc_poller_free(worker->nfc_poller);
         worker->nfc_poller = NULL;
         break;
      }
      case NfcComparatorCompareReaderWorkerState_Comparing: {
         nfc_comparator_compare_worker_compare_cards(
            worker->compare_worker, worker->scanned_nfc_card, worker->loaded_nfc_card);

         nfc_device_free(worker->scanned_nfc_card);
         worker->scanned_nfc_card = NULL;
         nfc_device_free(worker->loaded_nfc_card);
         worker->loaded_nfc_card = NULL;

         worker->state = NfcComparatorCompareReaderWorkerState_Stopped;
         break;
      }
      default:
         break;
      }
   }
   return 0;
}

NfcComparatorCompareReaderWorker*
   nfc_comparator_compare_reader_worker_alloc(NfcComparatorCompareWorker* compare_worker) {
   NfcComparatorCompareReaderWorker* worker = calloc(1, sizeof(NfcComparatorCompareReaderWorker));
   if(!worker) {
      return NULL;
   }
   worker->nfc = nfc_alloc();
   if(!worker->nfc) {
      free(worker);
      return NULL;
   }

   worker->compare_worker = compare_worker;

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "NfcComparatorCompareReaderWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 4096);
   furi_thread_set_callback(worker->thread, nfc_comparator_compare_reader_worker_task);

   if(!worker->thread) {
      nfc_free(worker->nfc);
      free(worker);
      return NULL;
   }
   worker->state = NfcComparatorCompareReaderWorkerState_Stopped;
   worker->loaded_nfc_card = NULL;
   worker->scanned_nfc_card = NULL;
   worker->protocol = NULL;
   return worker;
}

void nfc_comparator_compare_reader_worker_free(NfcComparatorCompareReaderWorker* worker) {
   furi_assert(worker);
   nfc_free(worker->nfc);
   furi_thread_free(worker->thread);
   if(worker->nfc_scanner) {
      nfc_scanner_free(worker->nfc_scanner);
      worker->nfc_scanner = NULL;
   }
   if(worker->nfc_poller) {
      nfc_poller_free(worker->nfc_poller);
      worker->nfc_poller = NULL;
   }
   if(worker->loaded_nfc_card) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
   }
   if(worker->scanned_nfc_card) {
      nfc_device_free(worker->scanned_nfc_card);
      worker->scanned_nfc_card = NULL;
   }
   free(worker);
}

void nfc_comparator_compare_reader_worker_stop(NfcComparatorCompareReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcComparatorCompareReaderWorkerState_Stopped) {
      worker->state = NfcComparatorCompareReaderWorkerState_Stopped;
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_compare_reader_worker_start(NfcComparatorCompareReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorCompareReaderWorkerState_Stopped) {
      worker->state = NfcComparatorCompareReaderWorkerState_Scanning;
      furi_thread_start(worker->thread);
   }
}

bool nfc_comparator_compare_reader_worker_set_loaded_nfc_card(
   NfcComparatorCompareReaderWorker* worker,
   const char* path_to_nfc_card) {
   furi_assert(worker);
   if(worker->loaded_nfc_card) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
   }
   worker->loaded_nfc_card = nfc_device_alloc();
   if(!worker->loaded_nfc_card) {
      return false;
   }
   if(!nfc_device_load(worker->loaded_nfc_card, path_to_nfc_card)) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
      return false;
   }
   return true;
}

const NfcComparatorCompareReaderWorkerState*
   nfc_comparator_compare_reader_worker_get_state(NfcComparatorCompareReaderWorker* worker) {
   furi_assert(worker);
   return &worker->state;
}
