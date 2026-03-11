#include "nfc_comparator_finder_worker_i.h"

static void nfc_comparator_finder_worker_scanner_callback(NfcScannerEvent event, void* context) {
   furi_assert(context);
   NfcComparatorFinderWorker* nfc_comparator_finder_worker = context;
   switch(event.type) {
   case NfcScannerEventTypeDetected:
      nfc_comparator_finder_worker->protocol = event.data.protocols;
      nfc_comparator_finder_worker->state = NfcComparatorFinderWorkerState_Polling;
      break;
   default:
      break;
   }
}

static NfcCommand
   nfc_comparator_finder_worker_poller_callback(NfcGenericEvent event, void* context) {
   UNUSED(event);
   furi_assert(context);
   NfcComparatorFinderWorker* nfc_comparator_finder_worker = context;
   nfc_comparator_finder_worker->state = NfcComparatorFinderWorkerState_Finding;
   return NfcCommandStop;
}

static int32_t nfc_comparator_finder_worker_task(void* context) {
   NfcComparatorFinderWorker* worker = context;
   furi_assert(worker);
   while(worker->state != NfcComparatorFinderWorkerState_Stopped) {
      switch(worker->state) {
      case NfcComparatorFinderWorkerState_Scanning: {
         NfcScanner* nfc_scanner = nfc_scanner_alloc(worker->nfc);
         nfc_scanner_start(nfc_scanner, nfc_comparator_finder_worker_scanner_callback, worker);
         while(worker->state == NfcComparatorFinderWorkerState_Scanning) {
            furi_delay_ms(100);
         }
         nfc_scanner_stop(nfc_scanner);
         break;
      }
      case NfcComparatorFinderWorkerState_Polling: {
         NfcPoller* nfc_poller = nfc_poller_alloc(worker->nfc, worker->protocol[0]);
         nfc_poller_start(nfc_poller, nfc_comparator_finder_worker_poller_callback, worker);
         while(worker->state == NfcComparatorFinderWorkerState_Polling) {
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
      case NfcComparatorFinderWorkerState_Finding: {
         nfc_comparator_finder_worker_compare_cards(
            worker->compare_worker, worker->scanned_nfc_card, worker->settings, NULL);
         worker->state = NfcComparatorFinderWorkerState_Stopped;
         break;
      }
      default:
         break;
      }
   }
   return 0;
}

NfcComparatorFinderWorker* nfc_comparator_finder_worker_alloc(
   NfcComparatorCompareWorker* compare_worker,
   NfcComparatorFinderWorkerSettings* settings) {
   NfcComparatorFinderWorker* worker = calloc(1, sizeof(NfcComparatorFinderWorker));
   if(!worker) return NULL;
   worker->nfc = nfc_alloc();
   if(!worker->nfc) {
      free(worker);
      return NULL;
   }

   worker->compare_worker = compare_worker;
   worker->settings = settings;

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "NfcComparatorFinderWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 4096);
   furi_thread_set_callback(worker->thread, nfc_comparator_finder_worker_task);

   if(!worker->thread) {
      nfc_free(worker->nfc);
      free(worker);
      return NULL;
   }
   worker->state = NfcComparatorFinderWorkerState_Stopped;
   worker->scanned_nfc_card = NULL;
   worker->loaded_nfc_card = nfc_device_alloc();
   worker->protocol = NULL;

   worker->dir_walk = dir_walk_alloc(furi_record_open(RECORD_STORAGE));
   return worker;
}

void nfc_comparator_finder_worker_free(NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   nfc_free(worker->nfc);
   furi_thread_free(worker->thread);
   if(worker->loaded_nfc_card) {
      nfc_device_free(worker->loaded_nfc_card);
      worker->loaded_nfc_card = NULL;
   }
   if(worker->scanned_nfc_card) {
      nfc_device_free(worker->scanned_nfc_card);
      worker->scanned_nfc_card = NULL;
   }
   if(worker->dir_walk) {
      dir_walk_free(worker->dir_walk);
      worker->dir_walk = NULL;
   }
   free(worker);
}

void nfc_comparator_finder_worker_stop(NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcComparatorFinderWorkerState_Stopped) {
      worker->state = NfcComparatorFinderWorkerState_Stopped;
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_finder_worker_start(NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorFinderWorkerState_Stopped) {
      worker->state = NfcComparatorFinderWorkerState_Scanning;
      furi_thread_start(worker->thread);
   }
}

NfcComparatorFinderWorkerState*
   nfc_comparator_finder_worker_get_state(NfcComparatorFinderWorker* worker) {
   furi_assert(worker);
   return &worker->state;
}

void nfc_comparator_finder_worker_compare_cards(
   NfcComparatorCompareWorker* compare_worker,
   NfcDevice* nfc_card_1,
   NfcComparatorFinderWorkerSettings* settings,
   FuriString* nfc_card_path) {
   furi_assert(nfc_card_1);
   DirWalk* dir_walk = dir_walk_alloc(furi_record_open(RECORD_STORAGE));
   NfcDevice* nfc_card_2 = nfc_device_alloc();
   NfcComparatorCompareWorker* tmp_compare_worker = nfc_comparator_compare_worker_alloc();

   if(dir_walk_open(dir_walk, "/ext/nfc")) {
      FuriString* ext = furi_string_alloc();

      dir_walk_set_recursive(dir_walk, settings->recursive);

      while(dir_walk_read(dir_walk, compare_worker->nfc_card_path, NULL) == DirWalkOK) {
         if(nfc_card_path && furi_string_cmpi(compare_worker->nfc_card_path, nfc_card_path) == 0) {
            NfcCompareWorkerType type = compare_worker->compare_type;
            nfc_comparator_compare_worker_reset(compare_worker);
            compare_worker->compare_type = type;
            continue;
         }

         path_extract_ext_str(compare_worker->nfc_card_path, ext);

         if(furi_string_cmpi_str(ext, ".nfc") == 0) {
            if(nfc_device_load(nfc_card_2, furi_string_get_cstr(compare_worker->nfc_card_path))) {
               nfc_comparator_compare_worker_compare_cards(compare_worker, nfc_card_1, nfc_card_2);

               if(compare_worker->results.uid && compare_worker->results.uid_length &&
                  compare_worker->results.protocol) {
                  if(compare_worker->diff.count == 0) {
                     break;
                  } else if(
                     furi_string_empty(tmp_compare_worker->nfc_card_path) ||
                     (tmp_compare_worker->diff.count > compare_worker->diff.count)) {
                     nfc_comparator_compare_worker_copy(tmp_compare_worker, compare_worker);
                  }
               } else {
                  NfcCompareWorkerType type = compare_worker->compare_type;
                  nfc_comparator_compare_worker_reset(compare_worker);
                  compare_worker->compare_type = type;
               }
            }
         }
      }

      if(!furi_string_empty(tmp_compare_worker->nfc_card_path) &&
         (furi_string_empty(compare_worker->nfc_card_path) ||
          tmp_compare_worker->diff.count < compare_worker->diff.count)) {
         nfc_comparator_compare_worker_copy(compare_worker, tmp_compare_worker);
      }

      nfc_comparator_compare_worker_free(tmp_compare_worker);

      dir_walk_close(dir_walk);
      furi_string_free(ext);
      dir_walk_free(dir_walk);
   }
   nfc_device_free(nfc_card_2);
}
