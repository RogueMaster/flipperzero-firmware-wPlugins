#include "k2_worker.h"
#include "../crypto/k2_crypto.h"
#include <furi_hal.h>
#include <storage/storage.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#define TAG         "K2Worker"
#define CFS_NFC_DIR EXT_PATH("nfc/CFS")

struct K2Worker {
    FuriThread* thread;
    FuriMutex* mutex;
    Nfc* nfc;
    K2WorkerMode mode;
    K2WorkerCallback callback;
    void* callback_context;

    K2SpoolConfig config;
    K2SpoolInfo last_info;
    volatile bool is_busy;
};

static uint64_t bytes_to_key_num(const uint8_t key[6]) {
    uint64_t val = 0;
    for(int i = 0; i < 6; i++) {
        val = (val << 8) | key[i];
    }
    return val;
}

void k2_prepare_mf_classic_data(
    const K2SpoolConfig* config,
    const uint8_t* optional_uid,
    MfClassicData* data) {
    furi_check(config);
    furi_check(data);

    /* Zero out all data and base ISO14443-3A data to prevent any garbage memory */
    Iso14443_3aData* iso = data->iso14443_3a_data;
    memset(data, 0, sizeof(MfClassicData));
    data->iso14443_3a_data = iso;
    if(iso) {
        memset(iso, 0, sizeof(Iso14443_3aData));
    }

    uint8_t uid[4];
    if(optional_uid) {
        memcpy(uid, optional_uid, 4);
    } else {
        furi_hal_random_fill_buf(uid, 4);
        if(uid[0] == 0x88 || uid[0] == 0x00) uid[0] = 0x12;
    }

    uint8_t enc_key[6];
    k2_crypto_derive_key(uid, enc_key);

    uint8_t s1_plain[48];
    uint8_t s2_plain[48];
    k2_build_payload(config, s1_plain, s2_plain);

    uint8_t s1_enc[48];
    k2_crypto_encrypt_sector1(s1_plain, s1_enc);

    data->type = MfClassicType1k;
    mf_classic_set_uid(data, uid, 4);

    if(iso) {
        iso->atqa[0] = 0x04;
        iso->atqa[1] = 0x00;
        iso->sak = 0x08;
    }

    /* Block 0: Manufacturer block */
    MfClassicBlock b0;
    memset(b0.data, 0, 16);
    b0.data[0] = uid[0];
    b0.data[1] = uid[1];
    b0.data[2] = uid[2];
    b0.data[3] = uid[3];
    b0.data[4] = uid[0] ^ uid[1] ^ uid[2] ^ uid[3]; /* BCC */
    b0.data[5] = 0x08; /* SAK */
    b0.data[6] = 0x04; /* ATQA0 */
    b0.data[7] = 0x00; /* ATQA1 */
    const uint8_t mfg_tail[8] = {0xE1, 0x10, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(&b0.data[8], mfg_tail, 8);
    mf_classic_set_block_read(data, 0, &b0);

    /* Zero block */
    MfClassicBlock zero_blk;
    memset(zero_blk.data, 0, 16);

    /* Default transport trailer (FF FF FF FF FF FF FF 07 80 69 FF FF FF FF FF FF) */
    MfClassicSectorTrailer def_trailer;
    memset(def_trailer.key_a.data, 0xFF, 6);
    def_trailer.access_bits.data[0] = 0xFF;
    def_trailer.access_bits.data[1] = 0x07;
    def_trailer.access_bits.data[2] = 0x80;
    def_trailer.access_bits.data[3] = 0x69;
    memset(def_trailer.key_b.data, 0xFF, 6);

    uint64_t def_key_num = 0x0000FFFFFFFFFFFFULL;
    uint64_t enc_key_num = bytes_to_key_num(enc_key);

    /* Sector 0: blocks 1, 2 = 0; trailer (block 3) = def_trailer */
    mf_classic_set_block_read(data, 1, &zero_blk);
    mf_classic_set_block_read(data, 2, &zero_blk);
    mf_classic_set_sector_trailer_read(data, 3, &def_trailer);
    mf_classic_set_key_found(data, 0, MfClassicKeyTypeA, def_key_num);
    mf_classic_set_key_found(data, 0, MfClassicKeyTypeB, def_key_num);

    /* Sector 1: blocks 4, 5, 6 = encrypted data */
    MfClassicBlock b4, b5, b6;
    memcpy(b4.data, s1_enc + 0, 16);
    memcpy(b5.data, s1_enc + 16, 16);
    memcpy(b6.data, s1_enc + 32, 16);
    mf_classic_set_block_read(data, 4, &b4);
    mf_classic_set_block_read(data, 5, &b5);
    mf_classic_set_block_read(data, 6, &b6);

    /* Sector 1 trailer (block 7): derived keys */
    MfClassicSectorTrailer s1_trailer;
    memcpy(s1_trailer.key_a.data, enc_key, 6);
    s1_trailer.access_bits.data[0] = 0xFF;
    s1_trailer.access_bits.data[1] = 0x07;
    s1_trailer.access_bits.data[2] = 0x80;
    s1_trailer.access_bits.data[3] = 0x69;
    memcpy(s1_trailer.key_b.data, enc_key, 6);
    mf_classic_set_sector_trailer_read(data, 7, &s1_trailer);
    mf_classic_set_key_found(data, 1, MfClassicKeyTypeA, enc_key_num);
    mf_classic_set_key_found(data, 1, MfClassicKeyTypeB, enc_key_num);

    /* Sector 2: blocks 8, 9, 10 = plaintext model */
    MfClassicBlock b8, b9, b10;
    memcpy(b8.data, s2_plain + 0, 16);
    memcpy(b9.data, s2_plain + 16, 16);
    memcpy(b10.data, s2_plain + 32, 16);
    mf_classic_set_block_read(data, 8, &b8);
    mf_classic_set_block_read(data, 9, &b9);
    mf_classic_set_block_read(data, 10, &b10);

    /* Sector 2 trailer (block 11): default keys */
    mf_classic_set_sector_trailer_read(data, 11, &def_trailer);
    mf_classic_set_key_found(data, 2, MfClassicKeyTypeA, def_key_num);
    mf_classic_set_key_found(data, 2, MfClassicKeyTypeB, def_key_num);

    /* Sectors 3 to 15: empty with default keys */
    for(uint8_t s = 3; s < 16; s++) {
        uint8_t first_blk = s * 4;
        mf_classic_set_block_read(data, first_blk + 0, &zero_blk);
        mf_classic_set_block_read(data, first_blk + 1, &zero_blk);
        mf_classic_set_block_read(data, first_blk + 2, &zero_blk);
        mf_classic_set_sector_trailer_read(data, first_blk + 3, &def_trailer);
        mf_classic_set_key_found(data, s, MfClassicKeyTypeA, def_key_num);
        mf_classic_set_key_found(data, s, MfClassicKeyTypeB, def_key_num);
    }
}

bool k2_worker_save_spool_to_nfc(
    const K2SpoolConfig* config,
    const uint8_t* optional_uid,
    char* out_filepath,
    size_t out_filepath_size) {
    if(!config) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, CFS_NFC_DIR);
    furi_record_close(RECORD_STORAGE);

    const K2Material* mat = k2_db_find_material_by_id(config->material_id);
    const char* mat_name = (mat && mat->name) ? mat->name : "Hyper PLA";
    const char* col_name = k2_db_find_closest_color_name(config->color_hex);
    if(!col_name || strlen(col_name) == 0) {
        col_name = "White";
    }

    char clean_name[32] = {0};
    for(size_t i = 0, j = 0; i < strlen(mat_name) && j < sizeof(clean_name) - 1; i++) {
        char c = mat_name[i];
        if(isalnum((unsigned char)c)) {
            clean_name[j++] = c;
        } else if(c == ' ' || c == '-') {
            clean_name[j++] = '_';
        }
    }

    char filename[64];
    snprintf(
        filename,
        sizeof(filename),
        "%s_%s_%s.nfc",
        clean_name,
        col_name,
        config->serial[0] ? config->serial : "000001");
    if(out_filepath) {
        snprintf(out_filepath, out_filepath_size, "%s/%s", CFS_NFC_DIR, filename);
    }

    MfClassicData* mf_data = mf_classic_alloc();
    k2_prepare_mf_classic_data(config, optional_uid, mf_data);

    NfcDevice* device = nfc_device_alloc();
    nfc_device_set_data(device, NfcProtocolMfClassic, (const NfcDeviceData*)mf_data);

    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", CFS_NFC_DIR, filename);
    bool saved = nfc_device_save(device, fullpath);

    nfc_device_free(device);
    mf_classic_free(mf_data);
    return saved;
}

bool k2_worker_load_spool_from_nfc(const char* filepath, K2SpoolInfo* info_out) {
    if(!filepath || !info_out) return false;

    NfcDevice* device = nfc_device_alloc();
    if(!nfc_device_load(device, filepath)) {
        nfc_device_free(device);
        return false;
    }

    if(nfc_device_get_protocol(device) != NfcProtocolMfClassic) {
        nfc_device_free(device);
        return false;
    }

    const MfClassicData* mf_data =
        (const MfClassicData*)nfc_device_get_data(device, NfcProtocolMfClassic);
    if(!mf_data) {
        nfc_device_free(device);
        return false;
    }

    /* Extract UID */
    size_t uid_len = 0;
    const uint8_t* uid_ptr = mf_classic_get_uid(mf_data, &uid_len);
    if(uid_ptr && uid_len >= 4) {
        memcpy(info_out->uid, uid_ptr, 4);
    }

    /* Extract Sector 1 blocks 4, 5, 6 */
    uint8_t cipher[48];
    memcpy(cipher + 0, mf_data->block[4].data, 16);
    memcpy(cipher + 16, mf_data->block[5].data, 16);
    memcpy(cipher + 32, mf_data->block[6].data, 16);

    uint8_t plain_s1[48];
    k2_crypto_decrypt_sector1(cipher, plain_s1);

    /* Extract Sector 2 blocks 8, 9, 10 */
    uint8_t plain_s2[48];
    memcpy(plain_s2 + 0, mf_data->block[8].data, 16);
    memcpy(plain_s2 + 16, mf_data->block[9].data, 16);
    memcpy(plain_s2 + 32, mf_data->block[10].data, 16);

    bool parsed = k2_parse_payload(plain_s1, plain_s2, info_out);
    nfc_device_free(device);
    return parsed;
}

static int32_t k2_worker_thread_func(void* context) {
    K2Worker* worker = context;

    Iso14443_3aData* iso3a = iso14443_3a_alloc();
    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

    while(true) {
        furi_mutex_acquire(worker->mutex, FuriWaitForever);
        K2WorkerMode mode = worker->mode;
        furi_mutex_release(worker->mutex);

        if(mode == K2WorkerModeStop) {
            break;
        }

        if(mode == K2WorkerModeScan) {
            worker->is_busy = true;
            Iso14443_3aError iso_err = iso14443_3a_poller_sync_read(worker->nfc, iso3a);
            if(iso_err == Iso14443_3aErrorNone && iso3a->uid_len >= 4) {
                if(worker->callback)
                    worker->callback(K2WorkerEventCardDetected, worker->callback_context);

                uint8_t derived_key_bytes[6];
                k2_crypto_derive_key(iso3a->uid, derived_key_bytes);
                MfClassicKey derived_key;
                memcpy(derived_key.data, derived_key_bytes, 6);

                MfClassicBlock b4, b5, b6, b8, b9, b10;
                MfClassicError err;

                /* Try authenticating and reading block 4 with derived key */
                err = mf_classic_poller_sync_read_block(
                    worker->nfc, 4, &derived_key, MfClassicKeyTypeA, &b4);
                bool encrypted = true;
                if(err != MfClassicErrorNone) {
                    /* Try default transport key */
                    err = mf_classic_poller_sync_read_block(
                        worker->nfc, 4, &default_key, MfClassicKeyTypeA, &b4);
                    encrypted = false;
                }

                if(err == MfClassicErrorNone) {
                    MfClassicKey* s1_key = encrypted ? &derived_key : &default_key;
                    mf_classic_poller_sync_read_block(
                        worker->nfc, 5, s1_key, MfClassicKeyTypeA, &b5);
                    mf_classic_poller_sync_read_block(
                        worker->nfc, 6, s1_key, MfClassicKeyTypeA, &b6);

                    uint8_t s1_raw[48];
                    memcpy(s1_raw + 0, b4.data, 16);
                    memcpy(s1_raw + 16, b5.data, 16);
                    memcpy(s1_raw + 32, b6.data, 16);

                    uint8_t s1_plain[48];
                    if(encrypted) {
                        k2_crypto_decrypt_sector1(s1_raw, s1_plain);
                    } else {
                        memcpy(s1_plain, s1_raw, 48);
                    }

                    /* Read Sector 2 with default key */
                    memset(b8.data, ' ', 16);
                    memset(b9.data, ' ', 16);
                    memset(b10.data, ' ', 16);
                    mf_classic_poller_sync_read_block(
                        worker->nfc, 8, &default_key, MfClassicKeyTypeA, &b8);
                    mf_classic_poller_sync_read_block(
                        worker->nfc, 9, &default_key, MfClassicKeyTypeA, &b9);
                    mf_classic_poller_sync_read_block(
                        worker->nfc, 10, &default_key, MfClassicKeyTypeA, &b10);

                    uint8_t s2_plain[48];
                    memcpy(s2_plain + 0, b8.data, 16);
                    memcpy(s2_plain + 16, b9.data, 16);
                    memcpy(s2_plain + 32, b10.data, 16);

                    furi_mutex_acquire(worker->mutex, FuriWaitForever);
                    k2_parse_payload(s1_plain, s2_plain, &worker->last_info);
                    memcpy(worker->last_info.uid, iso3a->uid, 4);
                    worker->mode = K2WorkerModeIdle;
                    furi_mutex_release(worker->mutex);

                    worker->is_busy = false;
                    if(worker->callback)
                        worker->callback(K2WorkerEventSuccess, worker->callback_context);
                } else {
                    worker->is_busy = false;
                    if(worker->callback)
                        worker->callback(K2WorkerEventAuthFailed, worker->callback_context);
                    furi_delay_ms(500);
                }
            } else {
                worker->is_busy = false;
                furi_delay_ms(100);
            }
        } else if(mode == K2WorkerModeWrite) {
            worker->is_busy = true;
            Iso14443_3aError iso_err = iso14443_3a_poller_sync_read(worker->nfc, iso3a);
            if(iso_err == Iso14443_3aErrorNone && iso3a->uid_len >= 4) {
                if(worker->callback)
                    worker->callback(K2WorkerEventCardDetected, worker->callback_context);

                uint8_t derived_key_bytes[6];
                k2_crypto_derive_key(iso3a->uid, derived_key_bytes);
                MfClassicKey derived_key;
                memcpy(derived_key.data, derived_key_bytes, 6);

                furi_mutex_acquire(worker->mutex, FuriWaitForever);
                K2SpoolConfig cfg = worker->config;
                furi_mutex_release(worker->mutex);

                uint8_t s1_plain[48];
                uint8_t s2_plain[48];
                k2_build_payload(&cfg, s1_plain, s2_plain);

                uint8_t s1_enc[48];
                k2_crypto_encrypt_sector1(s1_plain, s1_enc);

                MfClassicBlock b4, b5, b6, b7;
                memcpy(b4.data, s1_enc + 0, 16);
                memcpy(b5.data, s1_enc + 16, 16);
                memcpy(b6.data, s1_enc + 32, 16);

                /* Determine current Sector 1 auth key: try derived key, then default key */
                MfClassicBlock dummy;
                MfClassicError test_err = mf_classic_poller_sync_read_block(
                    worker->nfc, 4, &derived_key, MfClassicKeyTypeA, &dummy);
                bool already_encrypted = (test_err == MfClassicErrorNone);
                MfClassicKey* current_key = already_encrypted ? &derived_key : &default_key;

                /* Write blocks 4, 5, 6 */
                MfClassicError w_err = mf_classic_poller_sync_write_block(
                    worker->nfc, 4, current_key, MfClassicKeyTypeA, &b4);
                if(w_err == MfClassicErrorNone) {
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 5, current_key, MfClassicKeyTypeA, &b5);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 6, current_key, MfClassicKeyTypeA, &b6);

                    /* If tag was fresh, update trailer block 7 with derived keys */
                    if(!already_encrypted) {
                        memcpy(&b7.data[0], derived_key.data, 6);
                        b7.data[6] = 0xFF;
                        b7.data[7] = 0x07;
                        b7.data[8] = 0x80;
                        b7.data[9] = 0x69;
                        memcpy(&b7.data[10], derived_key.data, 6);
                        mf_classic_poller_sync_write_block(
                            worker->nfc, 7, current_key, MfClassicKeyTypeA, &b7);
                    }

                    /* Write Sector 2 (blocks 8, 9, 10) with printer model */
                    MfClassicBlock b8, b9, b10;
                    memcpy(b8.data, s2_plain + 0, 16);
                    memcpy(b9.data, s2_plain + 16, 16);
                    memcpy(b10.data, s2_plain + 32, 16);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 8, &default_key, MfClassicKeyTypeA, &b8);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 9, &default_key, MfClassicKeyTypeA, &b9);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 10, &default_key, MfClassicKeyTypeA, &b10);

                    furi_mutex_acquire(worker->mutex, FuriWaitForever);
                    worker->mode = K2WorkerModeIdle;
                    furi_mutex_release(worker->mutex);

                    worker->is_busy = false;
                    if(worker->callback)
                        worker->callback(K2WorkerEventSuccess, worker->callback_context);
                } else {
                    worker->is_busy = false;
                    if(worker->callback)
                        worker->callback(K2WorkerEventWriteFailed, worker->callback_context);
                    furi_delay_ms(500);
                }
            } else {
                worker->is_busy = false;
                furi_delay_ms(100);
            }
        } else if(mode == K2WorkerModeFormat) {
            worker->is_busy = true;
            Iso14443_3aError iso_err = iso14443_3a_poller_sync_read(worker->nfc, iso3a);
            if(iso_err == Iso14443_3aErrorNone && iso3a->uid_len >= 4) {
                if(worker->callback)
                    worker->callback(K2WorkerEventCardDetected, worker->callback_context);

                uint8_t derived_key_bytes[6];
                k2_crypto_derive_key(iso3a->uid, derived_key_bytes);
                MfClassicKey derived_key;
                memcpy(derived_key.data, derived_key_bytes, 6);

                MfClassicBlock dummy;
                MfClassicError test_err = mf_classic_poller_sync_read_block(
                    worker->nfc, 4, &derived_key, MfClassicKeyTypeA, &dummy);
                bool was_encrypted = (test_err == MfClassicErrorNone);
                MfClassicKey* auth_key = was_encrypted ? &derived_key : &default_key;

                MfClassicBlock zero_blk;
                memset(zero_blk.data, 0, 16);

                MfClassicError err = mf_classic_poller_sync_write_block(
                    worker->nfc, 4, auth_key, MfClassicKeyTypeA, &zero_blk);
                if(err == MfClassicErrorNone) {
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 5, auth_key, MfClassicKeyTypeA, &zero_blk);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 6, auth_key, MfClassicKeyTypeA, &zero_blk);

                    /* Reset trailer 7 to default transport keys */
                    if(was_encrypted) {
                        MfClassicBlock def_tr;
                        memset(&def_tr.data[0], 0xFF, 6);
                        def_tr.data[6] = 0xFF;
                        def_tr.data[7] = 0x07;
                        def_tr.data[8] = 0x80;
                        def_tr.data[9] = 0x69;
                        memset(&def_tr.data[10], 0xFF, 6);
                        mf_classic_poller_sync_write_block(
                            worker->nfc, 7, auth_key, MfClassicKeyTypeA, &def_tr);
                    }

                    /* Wipe Sector 2 */
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 8, &default_key, MfClassicKeyTypeA, &zero_blk);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 9, &default_key, MfClassicKeyTypeA, &zero_blk);
                    mf_classic_poller_sync_write_block(
                        worker->nfc, 10, &default_key, MfClassicKeyTypeA, &zero_blk);

                    furi_mutex_acquire(worker->mutex, FuriWaitForever);
                    worker->mode = K2WorkerModeIdle;
                    furi_mutex_release(worker->mutex);

                    worker->is_busy = false;
                    if(worker->callback)
                        worker->callback(K2WorkerEventSuccess, worker->callback_context);
                } else {
                    worker->is_busy = false;
                    if(worker->callback)
                        worker->callback(K2WorkerEventFormatFailed, worker->callback_context);
                    furi_delay_ms(500);
                }
            } else {
                worker->is_busy = false;
                furi_delay_ms(100);
            }
        } else {
            worker->is_busy = false;
            furi_delay_ms(100);
        }
    }

    iso14443_3a_free(iso3a);
    return 0;
}

K2Worker* k2_worker_alloc(void) {
    K2Worker* worker = malloc(sizeof(K2Worker));
    memset(worker, 0, sizeof(K2Worker));

    worker->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    worker->nfc = nfc_alloc();
    worker->mode = K2WorkerModeIdle;

    worker->thread = furi_thread_alloc();
    furi_thread_set_name(worker->thread, "K2NfcWorker");
    furi_thread_set_stack_size(worker->thread, 4096);
    furi_thread_set_context(worker->thread, worker);
    furi_thread_set_callback(worker->thread, k2_worker_thread_func);
    furi_thread_start(worker->thread);

    return worker;
}

void k2_worker_free(K2Worker* worker) {
    if(!worker) return;

    k2_worker_stop(worker);

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->mode = K2WorkerModeStop;
    furi_mutex_release(worker->mutex);

    furi_thread_join(worker->thread);
    furi_thread_free(worker->thread);

    nfc_free(worker->nfc);
    furi_mutex_free(worker->mutex);
    free(worker);
}

void k2_worker_set_callback(K2Worker* worker, K2WorkerCallback callback, void* context) {
    if(!worker) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->callback = callback;
    worker->callback_context = context;
    furi_mutex_release(worker->mutex);
}

void k2_worker_start_scan(K2Worker* worker) {
    if(!worker) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->mode = K2WorkerModeScan;
    furi_mutex_release(worker->mutex);
}

void k2_worker_start_write(K2Worker* worker, const K2SpoolConfig* config) {
    if(!worker) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    if(config) worker->config = *config;
    worker->mode = K2WorkerModeWrite;
    furi_mutex_release(worker->mutex);
}

void k2_worker_start_format(K2Worker* worker) {
    if(!worker) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->mode = K2WorkerModeFormat;
    furi_mutex_release(worker->mutex);
}

void k2_worker_stop(K2Worker* worker) {
    if(!worker) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->mode = K2WorkerModeIdle;
    furi_mutex_release(worker->mutex);

    while(worker->is_busy) {
        furi_delay_ms(10);
    }
}

Nfc* k2_worker_get_nfc(K2Worker* worker) {
    if(!worker) return NULL;
    return worker->nfc;
}

const K2SpoolInfo* k2_worker_get_last_info(const K2Worker* worker) {
    if(!worker) return NULL;
    return &worker->last_info;
}
