#include "nfc_comparator_finder_searcher_worker_i.h"

static int32_t nfc_comparator_finder_searcher_worker_task(void* context) {
   NfcComparatorFinderSearcherWorker* worker = context;
   NfcDevice* nfc_card_2 = nfc_device_alloc();
   NfcComparatorCompareWorker* tmp_compare_worker = nfc_comparator_compare_worker_alloc();

   if(dir_walk_open(worker->dir_walk, "/ext/nfc")) {
      FuriString* ext = furi_string_alloc();

      dir_walk_set_recursive(worker->dir_walk, worker->settings->recursive);

      int32_t read_count = 0;

      while(dir_walk_read(worker->dir_walk, worker->compare_worker->nfc_card_path, NULL) ==
            DirWalkOK) {
         if(worker->state == NfcComparatorFinderSearcherWorkerState_Stopped) {
            break;
         }

         if((++read_count % 16) == 0) {
            furi_delay_ms(1);
         }

         // skip self
         if(worker->nfc_card_path &&
            furi_string_cmpi(worker->compare_worker->nfc_card_path, worker->nfc_card_path) == 0) {
            NfcCompareWorkerType type = worker->compare_worker->compare_type;
            nfc_comparator_compare_worker_reset(worker->compare_worker);
            worker->compare_worker->compare_type = type;
            continue;
         }

         if(worker->state == NfcComparatorFinderSearcherWorkerState_Stopped) {
            break;
         }

         path_extract_ext_str(worker->compare_worker->nfc_card_path, ext);

         if(worker->state == NfcComparatorFinderSearcherWorkerState_Stopped) {
            break;
         }

         if(furi_string_cmpi_str(ext, ".nfc") == 0) {
            if(nfc_device_load(
                  nfc_card_2, furi_string_get_cstr(worker->compare_worker->nfc_card_path))) {
               if(worker->state == NfcComparatorFinderSearcherWorkerState_Stopped) {
                  break;
               }

               nfc_comparator_compare_worker_compare_cards(
                  worker->compare_worker, worker->nfc_card_1, nfc_card_2);

               if(worker->state == NfcComparatorFinderSearcherWorkerState_Stopped) {
                  break;
               }

               if(worker->compare_worker->results.uid &&
                  worker->compare_worker->results.uid_length &&
                  worker->compare_worker->results.protocol) {
                  if(worker->compare_worker->diff.count == 0) {
                     break;
                  } else if(
                     furi_string_empty(tmp_compare_worker->nfc_card_path) ||
                     (tmp_compare_worker->diff.count > worker->compare_worker->diff.count)) {
                     nfc_comparator_compare_worker_copy(
                        tmp_compare_worker, worker->compare_worker);
                  }

               } else {
                  NfcCompareWorkerType type = worker->compare_worker->compare_type;
                  nfc_comparator_compare_worker_reset(worker->compare_worker);
                  worker->compare_worker->compare_type = type;
               }
            }
         }
      }

      // final result selection
      if(!furi_string_empty(tmp_compare_worker->nfc_card_path) &&
         (furi_string_empty(worker->compare_worker->nfc_card_path) ||
          tmp_compare_worker->diff.count < worker->compare_worker->diff.count)) {
         nfc_comparator_compare_worker_copy(worker->compare_worker, tmp_compare_worker);
      }

      dir_walk_close(worker->dir_walk);
      furi_string_free(ext);
   }

   nfc_comparator_compare_worker_free(tmp_compare_worker);
   nfc_device_free(nfc_card_2);

   worker->state = NfcComparatorFinderSearcherWorkerState_Stopped;

   return 0;
}

NfcComparatorFinderSearcherWorker* nfc_comparator_finder_searcher_worker_alloc(
   NfcComparatorCompareWorker* compare_worker,
   NfcComparatorFinderSearcherWorkerSettings* settings,
   NfcDevice* nfc_card_1,
   FuriString* nfc_card_path) {
   NfcComparatorFinderSearcherWorker* worker =
      calloc(1, sizeof(NfcComparatorFinderSearcherWorker));
   if(!worker) return NULL;

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "NfcComparatorFinderSearcherWorker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_stack_size(worker->thread, 4096);
   furi_thread_set_callback(worker->thread, nfc_comparator_finder_searcher_worker_task);

   worker->compare_worker = compare_worker;

   worker->settings = settings;

   worker->state = NfcComparatorFinderSearcherWorkerState_Idle;

   worker->dir_walk = dir_walk_alloc(furi_record_open(RECORD_STORAGE));

   worker->nfc_card_1 = nfc_card_1;
   worker->nfc_card_path = nfc_card_path;

   return worker;
}

void nfc_comparator_finder_searcher_worker_free(NfcComparatorFinderSearcherWorker* worker) {
   furi_assert(worker);

   if(worker->state != NfcComparatorFinderSearcherWorkerState_Stopped) {
      furi_thread_join(worker->thread);
   }

   furi_thread_free(worker->thread);
   dir_walk_free(worker->dir_walk);

   free(worker);
}

void nfc_comparator_finder_searcher_worker_stop(NfcComparatorFinderSearcherWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorFinderSearcherWorkerState_Searching) {
      worker->state = NfcComparatorFinderSearcherWorkerState_Stopped;
      furi_thread_join(worker->thread);
   }
}

void nfc_comparator_finder_searcher_worker_start(NfcComparatorFinderSearcherWorker* worker) {
   furi_assert(worker);
   if(worker->state == NfcComparatorFinderSearcherWorkerState_Idle) {
      worker->state = NfcComparatorFinderSearcherWorkerState_Searching;
      furi_thread_start(worker->thread);
   }
}

NfcComparatorFinderSearcherWorkerState*
   nfc_comparator_finder_searcher_worker_get_state(NfcComparatorFinderSearcherWorker* worker) {
   static NfcComparatorFinderSearcherWorkerState stopped =
      NfcComparatorFinderSearcherWorkerState_Stopped;

   if(worker == NULL) {
      return &stopped;
   }

   return &worker->state;
}
