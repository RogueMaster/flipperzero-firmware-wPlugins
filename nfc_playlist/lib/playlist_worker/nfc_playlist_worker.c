#include "nfc_playlist_worker.h"

/**
 * Implements delay between NFC card emulations
 * @param worker Pointer to the NfcPlaylistWorker instance
 * @param playlist_position Current position in the playlist (0-based index)
 */
static void nfc_playlist_worker_delay(NfcPlaylistWorker* worker, int playlist_position) {
   if(!worker->settings->time_controls) {
      return;
   }
   worker->ms_counter = (options_emulate_delay[worker->settings->emulate_delay] * 1000);
   if(playlist_position < worker->settings->playlist_length && worker->ms_counter > 0 &&
      worker->state != NfcPlaylistWorkerState_Stopped &&
      worker->state != NfcPlaylistWorkerState_Skipping &&
      worker->state != NfcPlaylistWorkerState_Rewinding) {
      worker->state = NfcPlaylistWorkerState_Delaying;
      while(worker->ms_counter > 0 && worker->state == NfcPlaylistWorkerState_Delaying) {
         furi_delay_ms(10);
         worker->ms_counter -= 10;
      }
   }
   worker->ms_counter = 0;
}

/**
 * Handles countdown timer for various worker states
 * @param worker Pointer to the NfcPlaylistWorker instance
 * @param state The state to set during countdown (e.g., Emulating, Error states)
 */
static void
   nfc_playlist_worker_countdown(NfcPlaylistWorker* worker, NfcPlaylistWorkerState state) {
   worker->state = state;
   worker->ms_counter = (options_emulate_timeout[worker->settings->emulate_timeout] * 1000);
   while(worker->ms_counter > 0 && worker->state == state) {
      furi_delay_ms(10);
      if(worker->settings->time_controls) {
         worker->ms_counter -= 10;
      }
   }
   worker->ms_counter = 0;
}

/**
 * Main worker thread task that processes the NFC playlist
 * @param context Pointer to NfcPlaylistWorker instance (cast from void*)
 * @return int32_t Always returns 0 (success)
 */
static int32_t nfc_playlist_worker_task(void* context) {
   NfcPlaylistWorker* worker = context;
   furi_assert(worker);

   Storage* storage = furi_record_open(RECORD_STORAGE);
   Stream* stream = file_stream_alloc(storage);

   int playlist_position = 0;

   if(file_stream_open(
         stream,
         furi_string_get_cstr(worker->settings->playlist_path),
         FSAM_READ,
         FSOM_OPEN_EXISTING)) {
      FuriString* line = furi_string_alloc();

      while(worker->state != NfcPlaylistWorkerState_Stopped) {
         // Handle rewinding
         if(worker->state == NfcPlaylistWorkerState_Rewinding) {
            if(playlist_position == 1) {
               stream_rewind(stream);
               playlist_position = 0;
            } else {
               stream_rewind(stream);
               int current_pos = 0;
               int target_pos = playlist_position - 2;

               FuriString* temp_line = furi_string_alloc();
               while(current_pos < target_pos && stream_read_line(stream, temp_line) &&
                     worker->state != NfcPlaylistWorkerState_Stopped) {
                  furi_string_trim(temp_line);
                  if(!furi_string_empty(temp_line)) {
                     current_pos++;
                  }
               }
               furi_string_free(temp_line);

               playlist_position = current_pos;
            }
         }

         // Handle loop setting
         if(!stream_read_line(stream, line)) {
            if(worker->settings->loop) {
               stream_rewind(stream);
               playlist_position = 0;
               nfc_playlist_worker_delay(worker, playlist_position);
               continue;
            } else {
               break;
            }
         }

         furi_string_trim(line);
         if(furi_string_empty(line)) {
            continue;
         }

         furi_string_set(worker->nfc_card_path, furi_string_get_cstr(line));
         playlist_position++;

         // Check if the file has a valid NFC extension
         FuriString* file_ext = furi_string_alloc();
         path_extract_ext_str(worker->nfc_card_path, file_ext);
         if(furi_string_cmpi(file_ext, ".nfc") != 0) {
            furi_string_free(file_ext);
            if(worker->settings->skip_error) {
               continue;
            }

            nfc_playlist_worker_countdown(worker, NfcPlaylistWorkerState_InvalidFileType);
            nfc_playlist_worker_delay(worker, playlist_position);
            continue;
         }
         furi_string_free(file_ext);

         // Check if the file exists
         if(!storage_file_exists(storage, furi_string_get_cstr(worker->nfc_card_path))) {
            if(worker->settings->skip_error) {
               continue;
            }

            nfc_playlist_worker_countdown(worker, NfcPlaylistWorkerState_FileDoesNotExist);
            nfc_playlist_worker_delay(worker, playlist_position);
            continue;
         }

         // Load the NFC card and emulate it
         if(nfc_device_load(worker->nfc_device, furi_string_get_cstr(line))) {
            worker->nfc_protocol = nfc_device_get_protocol(worker->nfc_device);

            worker->nfc_listener = nfc_listener_alloc(
               worker->nfc,
               worker->nfc_protocol,
               nfc_device_get_data(worker->nfc_device, worker->nfc_protocol));
            nfc_listener_start(worker->nfc_listener, NULL, NULL);

            nfc_playlist_worker_countdown(worker, NfcPlaylistWorkerState_Emulating);

            nfc_listener_stop(worker->nfc_listener);
            nfc_listener_free(worker->nfc_listener);

            nfc_playlist_worker_delay(worker, playlist_position);
         } else {
            // Failed to load NFC card
            if(worker->settings->skip_error) {
               continue;
            }

            nfc_playlist_worker_countdown(worker, NfcPlaylistWorkerState_FailedToLoadNfcCard);
            nfc_playlist_worker_delay(worker, playlist_position);
         }
      }

      file_stream_close(stream);
      furi_string_free(line);
      worker->state = NfcPlaylistWorkerState_Stopped;
   } else {
      // Failed to load playlist
      worker->state = NfcPlaylistWorkerState_FailedToLoadPlaylist;
   }

   furi_record_close(RECORD_STORAGE);

   return 0;
}

/**
 * Allocates and initializes a new NFC playlist worker
 * @param settings Pointer to worker settings (timeout, delay, loop, etc.)
 * @return NfcPlaylistWorker* Pointer to newly allocated worker instance
 */
NfcPlaylistWorker* nfc_playlist_worker_alloc(NfcPlaylistWorkerSettings* settings) {
   NfcPlaylistWorker* worker = malloc(sizeof(NfcPlaylistWorker));
   furi_assert(worker);

   worker->thread = furi_thread_alloc();
   furi_thread_set_name(worker->thread, "nfc_playlist_worker");
   furi_thread_set_context(worker->thread, worker);
   furi_thread_set_callback(worker->thread, nfc_playlist_worker_task);
   furi_thread_set_stack_size(worker->thread, 4 * 1024);

   worker->state = NfcPlaylistWorkerState_Ready;
   worker->nfc = nfc_alloc();
   worker->nfc_device = nfc_device_alloc();
   worker->settings = settings;
   worker->nfc_card_path = furi_string_alloc();

   return worker;
}

/**
 * Frees all resources associated with the NFC playlist worker
 * @param worker Pointer to NfcPlaylistWorker instance to free
 */
void nfc_playlist_worker_free(NfcPlaylistWorker* worker) {
   furi_assert(worker);

   if(worker->nfc_device) {
      nfc_device_free(worker->nfc_device);
   }

   if(worker->nfc) {
      nfc_free(worker->nfc);
   }

   furi_thread_free(worker->thread);
   furi_string_free(worker->nfc_card_path);

   free(worker);
}

/**
 * Starts the NFC playlist worker thread
 * @param worker Pointer to NfcPlaylistWorker instance to start
 */
void nfc_playlist_worker_start(NfcPlaylistWorker* worker) {
   furi_assert(worker);
   furi_thread_start(worker->thread);
   worker->state = NfcPlaylistWorkerState_Emulating;
}

/**
 * Stops the NFC playlist worker and waits for thread completion
 * @param worker Pointer to NfcPlaylistWorker instance to stop
 */
void nfc_playlist_worker_stop(NfcPlaylistWorker* worker) {
   furi_assert(worker);
   worker->state = NfcPlaylistWorkerState_Stopped;
   furi_thread_join(worker->thread);
}

/**
 * Skips to the next NFC card in the playlist
 * @param worker Pointer to NfcPlaylistWorker instance to skip
 */
void nfc_playlist_worker_skip(NfcPlaylistWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcPlaylistWorkerState_Stopped ||
      worker->state != NfcPlaylistWorkerState_Ready) {
      worker->state = NfcPlaylistWorkerState_Skipping;
   }
}

/**
 * Rewinds the NFC playlist worker to the previous NFC card
 * @param worker Pointer to NfcPlaylistWorker instance to rewind
 */
void nfc_playlist_worker_rewind(NfcPlaylistWorker* worker) {
   furi_assert(worker);
   if(worker->state != NfcPlaylistWorkerState_Stopped ||
      worker->state != NfcPlaylistWorkerState_Ready) {
      worker->state = NfcPlaylistWorkerState_Rewinding;
   }
}
