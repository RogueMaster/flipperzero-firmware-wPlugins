#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include "../data/k2_database.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    K2WorkerModeIdle,
    K2WorkerModeScan,
    K2WorkerModeWrite,
    K2WorkerModeFormat,
    K2WorkerModeStop,
} K2WorkerMode;

typedef enum {
    K2WorkerEventCardDetected,
    K2WorkerEventSuccess,
    K2WorkerEventAuthFailed,
    K2WorkerEventReadFailed,
    K2WorkerEventWriteFailed,
    K2WorkerEventFormatFailed,
} K2WorkerEvent;

typedef void (*K2WorkerCallback)(K2WorkerEvent event, void* context);

typedef struct K2Worker K2Worker;

K2Worker* k2_worker_alloc(void);
void k2_worker_free(K2Worker* worker);

void k2_worker_set_callback(K2Worker* worker, K2WorkerCallback callback, void* context);

void k2_worker_start_scan(K2Worker* worker);
void k2_worker_start_write(K2Worker* worker, const K2SpoolConfig* config);
void k2_worker_start_format(K2Worker* worker);
void k2_worker_stop(K2Worker* worker);

Nfc* k2_worker_get_nfc(K2Worker* worker);
const K2SpoolInfo* k2_worker_get_last_info(const K2Worker* worker);

/* MIFARE Classic tag data builder */
void k2_prepare_mf_classic_data(const K2SpoolConfig* config, const uint8_t* optional_uid, MfClassicData* data);

/* Storage helpers for .nfc files */
bool k2_worker_save_spool_to_nfc(const K2SpoolConfig* config, const uint8_t* optional_uid, char* out_filepath, size_t out_filepath_size);
bool k2_worker_load_spool_from_nfc(const char* filepath, K2SpoolInfo* info_out);

#ifdef __cplusplus
}
#endif
