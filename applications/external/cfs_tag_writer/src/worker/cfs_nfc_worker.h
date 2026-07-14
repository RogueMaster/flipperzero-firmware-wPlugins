#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "src/data/cfs_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CfsNfcWorkerEventSuccess, // read/write completed; data available
    CfsNfcWorkerEventBlankTag, // valid 1K tag but CFS blocks are all zero
    CfsNfcWorkerEventNotPresent, // no tag presented within retry budget
    CfsNfcWorkerEventWrongType, // tag is not MIFARE Classic 1K
    CfsNfcWorkerEventAuthFailed, // could not authenticate sector 1
    CfsNfcWorkerEventReadFailed, // read/decrypt/parse failed
    CfsNfcWorkerEventWriteFailed, // write failed
    CfsNfcWorkerEventVerifyFailed, // write succeeded but read-back mismatch
} CfsNfcWorkerEvent;

typedef struct CfsNfcWorker CfsNfcWorker;

typedef void (*CfsNfcWorkerCallback)(CfsNfcWorkerEvent event, void* context);

CfsNfcWorker* cfs_nfc_worker_alloc(void);
void cfs_nfc_worker_free(CfsNfcWorker* worker);

void cfs_nfc_worker_set_callback(
    CfsNfcWorker* worker,
    CfsNfcWorkerCallback callback,
    void* context);

/** Start an async read. Result available via getters on Success/BlankTag. */
void cfs_nfc_worker_start_read(CfsNfcWorker* worker);

/** Start an async write of tag_data (copied internally). */
void cfs_nfc_worker_start_write(CfsNfcWorker* worker, const CfsTagData* tag_data);

/**
 * Start an async write of an exact 48-byte plaintext payload, bypassing the
 * lossy field encode. Used by "Write (clone)" to reproduce a saved tag's exact
 * payload. The payload is encrypted (fixed-key) and written to blocks 4/5/6 like
 * a normal write.
 */
void cfs_nfc_worker_start_write_raw(CfsNfcWorker* worker, const uint8_t* plain48);

/**
 * Start an async erase: zero the CFS data blocks (4/5/6) and reset the sector-1
 * trailer to factory keys (FFx6) + default access bits, returning the tag to a
 * blank/reusable state. Emits the same events as a write.
 */
void cfs_nfc_worker_start_erase(CfsNfcWorker* worker);

/** Stop and join the worker thread. Safe to call when idle. */
void cfs_nfc_worker_stop(CfsNfcWorker* worker);

/** Parsed tag data from the last successful read. */
const CfsTagData* cfs_nfc_worker_get_tag_data(const CfsNfcWorker* worker);

/** UID of the last tag seen. Returns NULL if none. */
const uint8_t* cfs_nfc_worker_get_uid(const CfsNfcWorker* worker, size_t* uid_len);

/** Decrypted 48-byte plaintext from the last successful read. */
const uint8_t* cfs_nfc_worker_get_read_plain(const CfsNfcWorker* worker);

#ifdef __cplusplus
}
#endif
