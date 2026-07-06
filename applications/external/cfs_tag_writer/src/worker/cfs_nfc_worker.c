#include "src/worker/cfs_nfc_worker.h"
#include "src/crypto/cfs_crypto.h"

#include <furi.h>
#include <string.h>

#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#define TAG "CfsNfcWorker"

// CFS data lives in sector 1: data blocks 4/5/6, sector trailer block 7.
#define CFS_BLOCK_FIRST    (4)
#define CFS_BLOCK_TRAILER  (7)
#define CFS_BLOCK_COUNT    (3)
#define CFS_MAX_ATTEMPTS   (3)
#define CFS_ATTEMPT_DELAY_MS (100)
// Overall time to wait for the user to present a tag before giving up.
#define CFS_WAIT_FOR_TAG_MS  (30000)
#define CFS_POLL_INTERVAL_MS (200)

// Sector-1 trailer access bits live in cfs_crypto (cfs_access_bits) so the
// writer and the .nfc dump builder share one definition.

// Factory-default MIFARE Classic key, tried as an auth fallback on blank tags.
// Not const: the mf_classic_poller_sync_* API takes MfClassicKey* (non-const).
static MfClassicKey cfs_key_factory_ff = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

// Key set for a write/erase pass: auth_a is tried first to write the data
// blocks (falling back to factory FF), trailer_a/trailer_b become the new
// sector trailer keys, and verify_key/verify_key_type re-read the tag
// afterward to confirm the write stuck.
typedef struct {
    MfClassicKey* auth_a;
    MfClassicKey* trailer_a;
    MfClassicKey* trailer_b;
    MfClassicKey* verify_key;
    MfClassicKeyType verify_key_type;
} CfsWriteKeys;

typedef enum {
    CfsNfcWorkerModeIdle,
    CfsNfcWorkerModeRead,
    CfsNfcWorkerModeWrite,
    CfsNfcWorkerModeErase,
} CfsNfcWorkerMode;

struct CfsNfcWorker {
    FuriThread* thread;
    CfsNfcWorkerMode mode;
    volatile bool stop;

    CfsNfcWorkerCallback callback;
    void* context;

    CfsTagData tag_data; // read result / write input
    uint8_t read_plain[CFS_PLAINTEXT_SIZE]; // decrypted payload from the last read
    uint8_t write_plain[CFS_PLAINTEXT_SIZE]; // 48-byte payload to write
    uint8_t uid[ISO14443_3A_MAX_UID_SIZE];
    size_t uid_len;
    bool has_uid;
};

static void cfs_emit(CfsNfcWorker* worker, CfsNfcWorkerEvent event) {
    // Drop events once a stop was requested: the scene is tearing down and does
    // not want a late result (e.g. cancelling a read makes the poll loop exit
    // with NotPresent). Prevents stale worker events reaching the next scene.
    if(worker->callback && !worker->stop) worker->callback(event, worker->context);
}

// Read blocks 4/5/6 with one key/type. Returns true only if all three read OK.
static bool cfs_try_read_blocks(
    Nfc* nfc,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    uint8_t out[CFS_PLAINTEXT_SIZE]) {
    for(int i = 0; i < CFS_BLOCK_COUNT; i++) {
        MfClassicBlock block;
        MfClassicError err = mf_classic_poller_sync_read_block(
            nfc, CFS_BLOCK_FIRST + i, key, key_type, &block);
        if(err != MfClassicErrorNone) return false;
        memcpy(out + i * MF_CLASSIC_BLOCK_SIZE, block.data, MF_CLASSIC_BLOCK_SIZE);
    }
    return true;
}

static bool cfs_try_write_blocks(
    Nfc* nfc,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    const uint8_t in[CFS_PLAINTEXT_SIZE]) {
    for(int i = 0; i < CFS_BLOCK_COUNT; i++) {
        MfClassicBlock block;
        memcpy(block.data, in + i * MF_CLASSIC_BLOCK_SIZE, MF_CLASSIC_BLOCK_SIZE);
        MfClassicError err = mf_classic_poller_sync_write_block(
            nfc, CFS_BLOCK_FIRST + i, key, key_type, &block);
        if(err != MfClassicErrorNone) return false;
    }
    return true;
}

// Poll for a 1K tag and capture the UID. Returns MfClassicErrorNone if a 1K
// card is present, otherwise a matching error code.
static MfClassicError cfs_poll_1k(CfsNfcWorker* worker, Nfc* nfc) {
    Iso14443_3aData iso_data;
    memset(&iso_data, 0, sizeof(iso_data));
    if(iso14443_3a_poller_sync_read(nfc, &iso_data) != Iso14443_3aErrorNone) {
        return MfClassicErrorNotPresent;
    }

    worker->uid_len = iso_data.uid_len;
    if(worker->uid_len > ISO14443_3A_MAX_UID_SIZE) worker->uid_len = ISO14443_3A_MAX_UID_SIZE;
    memcpy(worker->uid, iso_data.uid, worker->uid_len);
    worker->has_uid = true;

    MfClassicType type;
    MfClassicError err = mf_classic_poller_sync_detect_type(nfc, &type);
    if(err != MfClassicErrorNone) return err;
    if(type != MfClassicType1k) return MfClassicErrorProtocol; // signal wrong type
    return MfClassicErrorNone;
}

// Poll repeatedly until a 1K tag is presented, the wait times out, or stop.
static MfClassicError cfs_wait_for_1k(CfsNfcWorker* worker, Nfc* nfc) {
    uint32_t waited = 0;
    while(!worker->stop && waited < CFS_WAIT_FOR_TAG_MS) {
        MfClassicError e = cfs_poll_1k(worker, nfc);
        if(e == MfClassicErrorNone) return MfClassicErrorNone;
        if(e == MfClassicErrorProtocol) return e; // a tag is present but not 1K
        furi_delay_ms(CFS_POLL_INTERVAL_MS);
        waited += CFS_POLL_INTERVAL_MS;
    }
    return MfClassicErrorNotPresent;
}

static void cfs_do_read(CfsNfcWorker* worker) {
    Nfc* nfc = nfc_alloc();
    CfsNfcWorkerEvent result;

    MfClassicError poll = cfs_wait_for_1k(worker, nfc);
    if(poll == MfClassicErrorProtocol) {
        cfs_emit(worker, CfsNfcWorkerEventWrongType);
        nfc_free(nfc);
        return;
    }
    if(poll != MfClassicErrorNone) {
        cfs_emit(worker, CfsNfcWorkerEventNotPresent);
        nfc_free(nfc);
        return;
    }

    MfClassicKey key_a, key_b;
    cfs_crypto_derive_key_a(worker->uid, worker->uid_len, &key_a);
    cfs_crypto_derive_key_b(worker->uid, worker->uid_len, &key_b);

    uint8_t raw[CFS_PLAINTEXT_SIZE];
    bool ok = false;
    for(int attempt = 0; attempt < CFS_MAX_ATTEMPTS && !ok && !worker->stop; attempt++) {
        if(attempt > 0) furi_delay_ms(CFS_ATTEMPT_DELAY_MS);
        ok = cfs_try_read_blocks(nfc, &key_a, MfClassicKeyTypeA, raw) ||
             cfs_try_read_blocks(nfc, &cfs_key_factory_ff, MfClassicKeyTypeA, raw) ||
             cfs_try_read_blocks(nfc, &key_b, MfClassicKeyTypeB, raw) ||
             cfs_try_read_blocks(nfc, &cfs_key_factory_ff, MfClassicKeyTypeB, raw);
    }

    if(!ok) {
        cfs_emit(worker, CfsNfcWorkerEventAuthFailed);
        nfc_free(nfc);
        return;
    }

    if(cfs_data_is_blank(raw)) {
        cfs_emit(worker, CfsNfcWorkerEventBlankTag);
        nfc_free(nfc);
        return;
    }

    cfs_crypto_decrypt(raw, worker->read_plain);

    result = cfs_data_decode(worker->read_plain, &worker->tag_data) ?
                 CfsNfcWorkerEventSuccess :
                 CfsNfcWorkerEventReadFailed;
    nfc_free(nfc);
    cfs_emit(worker, result);
}

// Shared retry loop for both the write and erase paths: try the given auth
// candidate then blank FF to write the data blocks, rewrite the sector
// trailer with trailer_key_a/trailer_key_b, then read back with verify_key
// and compare against payload. Assumes a 1K tag is already present.
static CfsNfcWorkerEvent cfs_write_verify_blocks(
    CfsNfcWorker* worker,
    Nfc* nfc,
    const uint8_t payload[CFS_PLAINTEXT_SIZE],
    const CfsWriteKeys* keys,
    const char* log_prefix) {
    CfsNfcWorkerEvent result = CfsNfcWorkerEventWriteFailed;
    for(int attempt = 0; attempt < CFS_MAX_ATTEMPTS && !worker->stop; attempt++) {
        if(attempt > 0) furi_delay_ms(CFS_ATTEMPT_DELAY_MS);

        // Auth candidates: previously-written derived key first, then blank FF.
        MfClassicKey* wkey;
        if(cfs_try_write_blocks(nfc, keys->auth_a, MfClassicKeyTypeA, payload)) {
            wkey = keys->auth_a;
        } else if(cfs_try_write_blocks(nfc, &cfs_key_factory_ff, MfClassicKeyTypeA, payload)) {
            wkey = &cfs_key_factory_ff;
        } else {
            FURI_LOG_W(TAG, "%s: data block write failed (attempt %d)", log_prefix, attempt);
            result = CfsNfcWorkerEventWriteFailed;
            continue; // retry whole attempt
        }

        // Write sector trailer (block 7) with the target keys + access bits.
        MfClassicBlock trailer;
        memcpy(trailer.data, keys->trailer_a->data, MF_CLASSIC_KEY_SIZE);
        memcpy(trailer.data + MF_CLASSIC_KEY_SIZE, cfs_access_bits, MF_CLASSIC_ACCESS_BYTES_SIZE);
        memcpy(
            trailer.data + MF_CLASSIC_KEY_SIZE + MF_CLASSIC_ACCESS_BYTES_SIZE,
            keys->trailer_b->data,
            MF_CLASSIC_KEY_SIZE);
        MfClassicError terr = mf_classic_poller_sync_write_block(
            nfc, CFS_BLOCK_TRAILER, wkey, MfClassicKeyTypeA, &trailer);
        if(terr != MfClassicErrorNone) {
            FURI_LOG_W(TAG, "%s: trailer write failed (err %d)", log_prefix, terr);
            result = CfsNfcWorkerEventWriteFailed;
            continue;
        }

        // Verify: re-read with the now-active key and compare.
        uint8_t verify_raw[CFS_PLAINTEXT_SIZE];
        if(!cfs_try_read_blocks(nfc, keys->verify_key, keys->verify_key_type, verify_raw) ||
           memcmp(verify_raw, payload, CFS_PLAINTEXT_SIZE) != 0) {
            FURI_LOG_W(TAG, "%s: verify read-back mismatch", log_prefix);
            result = CfsNfcWorkerEventVerifyFailed;
            break;
        }

        result = CfsNfcWorkerEventSuccess;
        break;
    }
    return result;
}

// Write worker->write_plain to the (already-present) tag, then verify. Assumes
// cfs_wait_for_1k already succeeded so worker->uid is populated.
static CfsNfcWorkerEvent cfs_write_present_tag(CfsNfcWorker* worker, Nfc* nfc) {
    uint8_t cipher[CFS_PLAINTEXT_SIZE];
    cfs_crypto_encrypt(worker->write_plain, cipher);

    MfClassicKey key_a, key_b;
    cfs_crypto_derive_key_a(worker->uid, worker->uid_len, &key_a);
    cfs_crypto_derive_key_b(worker->uid, worker->uid_len, &key_b);

    CfsWriteKeys keys = {
        .auth_a = &key_a,
        .trailer_a = &key_a,
        .trailer_b = &key_b,
        .verify_key = &key_a,
        .verify_key_type = MfClassicKeyTypeA,
    };
    return cfs_write_verify_blocks(worker, nfc, cipher, &keys, "write");
}

static CfsNfcWorkerEvent cfs_present_and_write(CfsNfcWorker* worker, Nfc* nfc) {
    MfClassicError poll = cfs_wait_for_1k(worker, nfc);
    if(poll == MfClassicErrorProtocol) return CfsNfcWorkerEventWrongType;
    if(poll != MfClassicErrorNone) return CfsNfcWorkerEventNotPresent;
    return cfs_write_present_tag(worker, nfc);
}

static void cfs_do_write(CfsNfcWorker* worker) {
    Nfc* nfc = nfc_alloc();
    CfsNfcWorkerEvent result = cfs_present_and_write(worker, nfc);
    nfc_free(nfc);
    cfs_emit(worker, result);
}

// Zero blocks 4/5/6 and reset the sector trailer to factory FF keys + default
// access bits, so the tag reads back blank and is reusable. Assumes a 1K tag is
// already present (worker->uid populated).
static CfsNfcWorkerEvent cfs_erase_present_tag(CfsNfcWorker* worker, Nfc* nfc) {
    uint8_t zeros[CFS_PLAINTEXT_SIZE];
    memset(zeros, 0, sizeof(zeros));

    MfClassicKey key_a;
    cfs_crypto_derive_key_a(worker->uid, worker->uid_len, &key_a);

    // Reset trailer to factory: Key A = FFx6, access bits, Key B = FFx6.
    CfsWriteKeys keys = {
        .auth_a = &key_a,
        .trailer_a = &cfs_key_factory_ff,
        .trailer_b = &cfs_key_factory_ff,
        .verify_key = &cfs_key_factory_ff,
        .verify_key_type = MfClassicKeyTypeA,
    };
    return cfs_write_verify_blocks(worker, nfc, zeros, &keys, "erase");
}

static void cfs_do_erase(CfsNfcWorker* worker) {
    Nfc* nfc = nfc_alloc();
    MfClassicError poll = cfs_wait_for_1k(worker, nfc);
    CfsNfcWorkerEvent result;
    if(poll == MfClassicErrorProtocol) {
        result = CfsNfcWorkerEventWrongType;
    } else if(poll != MfClassicErrorNone) {
        result = CfsNfcWorkerEventNotPresent;
    } else {
        result = cfs_erase_present_tag(worker, nfc);
    }
    nfc_free(nfc);
    cfs_emit(worker, result);
}

static int32_t cfs_nfc_worker_thread(void* context) {
    CfsNfcWorker* worker = context;
    if(worker->mode == CfsNfcWorkerModeRead) {
        cfs_do_read(worker);
    } else if(worker->mode == CfsNfcWorkerModeWrite) {
        cfs_do_write(worker);
    } else if(worker->mode == CfsNfcWorkerModeErase) {
        cfs_do_erase(worker);
    }
    return 0;
}

CfsNfcWorker* cfs_nfc_worker_alloc(void) {
    CfsNfcWorker* worker = malloc(sizeof(CfsNfcWorker));
    memset(worker, 0, sizeof(CfsNfcWorker));
    worker->mode = CfsNfcWorkerModeIdle;
    worker->thread =
        furi_thread_alloc_ex("CfsNfcWorker", 4 * 1024, cfs_nfc_worker_thread, worker);
    return worker;
}

void cfs_nfc_worker_free(CfsNfcWorker* worker) {
    furi_assert(worker);
    cfs_nfc_worker_stop(worker);
    furi_thread_free(worker->thread);
    free(worker);
}

void cfs_nfc_worker_set_callback(
    CfsNfcWorker* worker,
    CfsNfcWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    worker->callback = callback;
    worker->context = context;
}

static void cfs_nfc_worker_start(CfsNfcWorker* worker, CfsNfcWorkerMode mode) {
    cfs_nfc_worker_stop(worker);
    worker->mode = mode;
    worker->stop = false;
    furi_thread_start(worker->thread);
}

void cfs_nfc_worker_start_read(CfsNfcWorker* worker) {
    furi_assert(worker);
    cfs_nfc_worker_start(worker, CfsNfcWorkerModeRead);
}

void cfs_nfc_worker_start_write(CfsNfcWorker* worker, const CfsTagData* tag_data) {
    furi_assert(worker);
    furi_assert(tag_data);
    worker->tag_data = *tag_data;
    cfs_data_encode(tag_data, worker->write_plain);
    cfs_nfc_worker_start(worker, CfsNfcWorkerModeWrite);
}

void cfs_nfc_worker_start_write_raw(CfsNfcWorker* worker, const uint8_t* plain48) {
    furi_assert(worker);
    furi_assert(plain48);
    memcpy(worker->write_plain, plain48, CFS_PLAINTEXT_SIZE);
    cfs_nfc_worker_start(worker, CfsNfcWorkerModeWrite);
}

void cfs_nfc_worker_start_erase(CfsNfcWorker* worker) {
    furi_assert(worker);
    cfs_nfc_worker_start(worker, CfsNfcWorkerModeErase);
}

void cfs_nfc_worker_stop(CfsNfcWorker* worker) {
    furi_assert(worker);
    worker->stop = true;
    furi_thread_join(worker->thread);
    worker->mode = CfsNfcWorkerModeIdle;
}

const CfsTagData* cfs_nfc_worker_get_tag_data(const CfsNfcWorker* worker) {
    return &worker->tag_data;
}

const uint8_t* cfs_nfc_worker_get_uid(const CfsNfcWorker* worker, size_t* uid_len) {
    if(!worker->has_uid) return NULL;
    if(uid_len) *uid_len = worker->uid_len;
    return worker->uid;
}

const uint8_t* cfs_nfc_worker_get_read_plain(const CfsNfcWorker* worker) {
    return worker->read_plain;
}
