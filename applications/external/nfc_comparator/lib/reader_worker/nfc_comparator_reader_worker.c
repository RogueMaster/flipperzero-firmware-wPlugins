#include "nfc_comparator_reader_worker_i.h"

static void nfc_comparator_reader_worker_scanner_callback(NfcScannerEvent event, void* context) {
   furi_assert(context);
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      nfc_comparator_reader_worker->protocol = event.data.protocols;
      nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Polling;
      break;
   default:
      break;
   }
}

static NfcCommand
   nfc_comparator_reader_worker_poller_callback(NfcGenericEvent event, void* context) {
   furi_assert(context);
   NfcComparatorReaderWorker* nfc_comparator_reader_worker = context;
   UNUSED(event);
   nfc_comparator_reader_worker->state = NfcComparatorReaderWorkerState_Comparing;
   return NfcCommandStop;
}

static int32_t nfc_comparator_reader_worker_task(void* context) {
   NfcComparatorReaderWorker* worker = context;
   furi_assert(worker);
   while(worker->state != NfcComparatorReaderWorkerState_Stopped) {
      switch(worker->state) {
      case NfcComparatorReaderWorkerState_Scanning: {
         worker->nfc_scanner = nfc_scanner_alloc(worker->nfc);
         nfc_scanner_start(
            worker->nfc_scanner, nfc_comparator_reader_worker_scanner_callback, worker);
         while(worker->state == NfcComparatorReaderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(worker->nfc_scanner);
         nfc_scanner_free(worker->nfc_scanner);
         worker->nfc_scanner = NULL;
         break;
      }
      case NfcComparatorReaderWorkerState_Polling: {
         worker->nfc_poller = nfc_poller_alloc(worker->nfc, worker->protocol[0]);
         nfc_poller_start(
            worker->nfc_poller, nfc_comparator_reader_worker_poller_callback, worker);
         while(worker->state == NfcComparatorReaderWorkerState_Polling) {
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
      case NfcComparatorReaderWorkerState_Comparing: {
         nfc_comparator_compare_worker_compare_cards(
            worker->compare_worker, worker->scanned_nfc_card, worker->loaded_nfc_card);

         nfc_device_free(worker->scanned_nfc_card);
         worker->scanned_nfc_card = NULL;
         nfc_device_free(worker->loaded_nfc_card);
         worker->loaded_nfc_card = NULL;

         worker->state = NfcComparatorReaderWorkerState_Stopped;
         break;
      }
      default:
         break;
      }
   }
   return 0;
}

NfcComparatorReaderWorker*
   nfc_comparator_reader_worker_alloc(NfcComparatorCompareWorker* compare_worker) {
   NfcComparatorReaderWorker* worker = calloc(1, sizeof(NfcComparatorReaderWorker));
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
   furi_thread_set_name(worker->thread, "NfcComparatorReaderWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 4096);
   furi_thread_set_callback(worker->thread, nfc_comparator_reader_worker_task);

   if(!worker->thread) {
      nfc_free(worker->nfc);
      free(worker);
      return NULL;
   }
   worker->state = NfcComparatorReaderWorkerState_Stopped;
   worker->loaded_nfc_card = NULL;
   worker->scanned_nfc_card = NULL;
   worker->protocol = NULL;
   return worker;
}

void nfc_comparator_reader_worker_free(NfcComparatorReaderWorker* worker) {
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

void nfc_comparator_reader_worker_stop(NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcComparatorReaderWorkerState_Stopped) {
      worker->state = NfcComparatorReaderWorkerState_Stopped;
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_reader_worker_start(NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorReaderWorkerState_Stopped) {
      worker->state = NfcComparatorReaderWorkerState_Scanning;
      furi_thread_start(worker->thread);
   }
}

bool nfc_comparator_reader_worker_set_loaded_nfc_card(
   NfcComparatorReaderWorker* worker,
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

const NfcComparatorReaderWorkerState*
   nfc_comparator_reader_worker_get_state(NfcComparatorReaderWorker* worker) {
   furi_assert(worker);
   return &worker->state;
}
