#pragma once

#include <nfc/protocols/nfc_protocol.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FuriThread FuriThread;
typedef struct FuriString FuriString;
typedef struct NfcListener NfcListener;
typedef struct NfcDevice NfcDevice;
typedef struct Nfc Nfc;

static const int options_emulate_timeout[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const int default_emulate_timeout = 4;
static const int options_emulate_delay[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const int default_emulate_delay = 0;
static const bool default_emulate_led_indicator = true;
static const bool default_skip_error = false;
static const bool default_loop = false;
static const bool default_time_controls = true;
static const bool default_user_controls = true;

typedef enum {
   NfcPlaylistWorkerState_Ready,
   NfcPlaylistWorkerState_Emulating,
   NfcPlaylistWorkerState_Delaying,
   NfcPlaylistWorkerState_Skipping,
   NfcPlaylistWorkerState_Rewinding,
   NfcPlaylistWorkerState_InvalidFileType,
   NfcPlaylistWorkerState_FileDoesNotExist,
   NfcPlaylistWorkerState_FailedToLoadPlaylist,
   NfcPlaylistWorkerState_FailedToLoadNfcCard,
   NfcPlaylistWorkerState_Stopped
} NfcPlaylistWorkerState;

typedef struct {
   FuriString* playlist_path;
   uint8_t playlist_length;
   uint8_t emulate_timeout;
   uint8_t emulate_delay;
   bool emulate_led_indicator;
   bool skip_error;
   bool loop;
   bool time_controls;
   bool user_controls;
} NfcPlaylistWorkerSettings;

typedef struct {
   FuriThread* thread;
   NfcPlaylistWorkerState state;
   NfcListener* nfc_listener;
   NfcDevice* nfc_device;
   NfcProtocol nfc_protocol;
   Nfc* nfc;
   NfcPlaylistWorkerSettings* settings;
   FuriString* nfc_card_path;
   int ms_counter;
} NfcPlaylistWorker;

/**
 * Allocates and initializes a new NFC playlist worker
 * @param settings Pointer to worker settings (timeout, delay, loop, etc.)
 * @return NfcPlaylistWorker* Pointer to newly allocated worker instance
 */
NfcPlaylistWorker* nfc_playlist_worker_alloc(NfcPlaylistWorkerSettings* settings);

/**
 * Frees all resources associated with the NFC playlist worker
 * @param worker Pointer to NfcPlaylistWorker instance to free
 */
void nfc_playlist_worker_free(NfcPlaylistWorker* worker);

/**
 * Stops the NFC playlist worker and waits for thread completion
 * @param worker Pointer to NfcPlaylistWorker instance to stop
 */
void nfc_playlist_worker_stop(NfcPlaylistWorker* worker);

/**
 * Starts the NFC playlist worker thread
 * @param worker Pointer to NfcPlaylistWorker instance to start
 */
void nfc_playlist_worker_start(NfcPlaylistWorker* worker);

/**
 * Skips to the next NFC card in the playlist
 * @param worker Pointer to NfcPlaylistWorker instance to skip
 */
void nfc_playlist_worker_skip(NfcPlaylistWorker* worker);

/**
 * Rewinds the NFC playlist worker to the previous NFC card
 * @param worker Pointer to NfcPlaylistWorker instance to rewind
 */
void nfc_playlist_worker_rewind(NfcPlaylistWorker* worker);

#ifdef __cplusplus
}
#endif
