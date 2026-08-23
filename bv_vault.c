#include "bv_vault.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "BioVaultVault"

#define KEYSTORE_PATH APP_DATA_PATH("keystore.bin")
#define KS_MAGIC "BVK1"
#define KS_MAGIC_LEN 4
#define KS_SIZE (KS_MAGIC_LEN + BV_WRAP_IV_SIZE + BV_DEK_SIZE) // 4 + 16 + 32 = 52

// On-tag blob framing.
#define BLOB_MAGIC0 'B'
#define BLOB_MAGIC1 'V'
#define BLOB_VER 1
#define OFF_NONCE 3
#define OFF_CTLEN (OFF_NONCE + BV_GCM_IV_SIZE) // 15
#define OFF_CT BV_BLOB_HEADER // 17

// --- Keystore file I/O ---

typedef enum {
    KsReadOk,
    KsReadMissing, // no keystore file: safe to create a fresh one
    KsReadBad, // file exists but is unreadable/short: must NOT re-key
} KsReadStatus;

static KsReadStatus ks_read(uint8_t* buf, size_t len) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    KsReadStatus status;
    if(storage_file_open(file, KEYSTORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        status = (storage_file_read(file, buf, len) == len) ? KsReadOk : KsReadBad;
        storage_file_close(file);
    } else {
        FileInfo info;
        status = (storage_common_stat(storage, KEYSTORE_PATH, &info) == FSE_NOT_EXIST) ?
                     KsReadMissing :
                     KsReadBad;
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return status;
}

static bool ks_write(const uint8_t* buf, size_t len) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH("")); // ensure app data dir exists
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, KEYSTORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, buf, len) == len;
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// --- Keystore ---

bool bv_vault_key_open(BvVaultKey* out) {
    if(!bv_crypto_kek_ensure()) {
        FURI_LOG_E(TAG, "KEK ensure failed");
        return false;
    }

    uint8_t ks[KS_SIZE];
    KsReadStatus status = ks_read(ks, sizeof(ks));
    if(status == KsReadOk && memcmp(ks, KS_MAGIC, KS_MAGIC_LEN) != 0) status = KsReadBad;
    if(status == KsReadOk) {
        const uint8_t* wrap_iv = ks + KS_MAGIC_LEN;
        const uint8_t* wrapped = ks + KS_MAGIC_LEN + BV_WRAP_IV_SIZE;
        bool ok = bv_crypto_kek_unwrap(wrap_iv, wrapped, out->dek);
        memset(ks, 0, sizeof(ks));
        if(!ok) FURI_LOG_E(TAG, "DEK unwrap failed (enclave key changed?)");
        return ok;
    }
    if(status == KsReadBad) {
        // A keystore that exists but doesn't parse is a torn write or SD
        // corruption, not a fresh device. Re-keying here would orphan the
        // on-tag vault forever, so refuse instead of generating a new DEK.
        FURI_LOG_E(TAG, "keystore present but unreadable/corrupt; refusing to re-key");
        return false;
    }

    // First use (no keystore file): generate a fresh random DEK, wrap it,
    // persist the keystore.
    uint8_t wrap_iv[BV_WRAP_IV_SIZE];
    uint8_t wrapped[BV_DEK_SIZE];
    furi_hal_random_fill_buf(out->dek, BV_DEK_SIZE);
    furi_hal_random_fill_buf(wrap_iv, sizeof(wrap_iv));
    if(!bv_crypto_kek_wrap(wrap_iv, out->dek, wrapped)) {
        FURI_LOG_E(TAG, "DEK wrap failed");
        return false;
    }
    memcpy(ks, KS_MAGIC, KS_MAGIC_LEN);
    memcpy(ks + KS_MAGIC_LEN, wrap_iv, BV_WRAP_IV_SIZE);
    memcpy(ks + KS_MAGIC_LEN + BV_WRAP_IV_SIZE, wrapped, BV_DEK_SIZE);
    bool ok = ks_write(ks, sizeof(ks));
    memset(ks, 0, sizeof(ks));
    memset(wrapped, 0, sizeof(wrapped));
    FURI_LOG_I(TAG, "created new keystore: %s", ok ? "OK" : "FAIL");
    return ok;
}

void bv_vault_key_clear(BvVaultKey* key) {
    memset(key, 0, sizeof(*key));
}

// --- Vault blob codec ---

bool bv_vault_encrypt(
    const BvVaultKey* key,
    const uint8_t* pt,
    size_t pt_len,
    uint8_t* blob,
    size_t* blob_len) {
    if(pt_len > 0xFFFF) return false;

    uint8_t nonce[BV_GCM_IV_SIZE];
    furi_hal_random_fill_buf(nonce, sizeof(nonce));

    blob[0] = BLOB_MAGIC0;
    blob[1] = BLOB_MAGIC1;
    blob[2] = BLOB_VER;
    memcpy(blob + OFF_NONCE, nonce, BV_GCM_IV_SIZE);
    blob[OFF_CTLEN] = (uint8_t)(pt_len & 0xFF);
    blob[OFF_CTLEN + 1] = (uint8_t)((pt_len >> 8) & 0xFF);

    uint8_t tag[BV_GCM_TAG_SIZE];
    if(!bv_crypto_gcm_seal(key->dek, nonce, pt, pt_len, blob + OFF_CT, tag)) return false;
    memcpy(blob + OFF_CT + pt_len, tag, BV_GCM_TAG_SIZE);

    *blob_len = OFF_CT + pt_len + BV_GCM_TAG_SIZE;
    return true;
}

bool bv_vault_framed_len(const uint8_t* blob, size_t avail, size_t* out_len) {
    if(avail < BV_BLOB_OVERHEAD) return false;
    if(blob[0] != BLOB_MAGIC0 || blob[1] != BLOB_MAGIC1 || blob[2] != BLOB_VER) return false;
    size_t ct_len = (size_t)blob[OFF_CTLEN] | ((size_t)blob[OFF_CTLEN + 1] << 8);
    size_t total = OFF_CT + ct_len + BV_GCM_TAG_SIZE;
    if(total > avail) return false;
    *out_len = total;
    return true;
}

bool bv_vault_decrypt(
    const BvVaultKey* key,
    const uint8_t* blob,
    size_t blob_len,
    uint8_t* pt,
    size_t pt_cap,
    size_t* pt_len) {
    if(blob_len < BV_BLOB_OVERHEAD) return false;
    if(blob[0] != BLOB_MAGIC0 || blob[1] != BLOB_MAGIC1 || blob[2] != BLOB_VER) return false;

    size_t ct_len = (size_t)blob[OFF_CTLEN] | ((size_t)blob[OFF_CTLEN + 1] << 8);
    if(blob_len != OFF_CT + ct_len + BV_GCM_TAG_SIZE) return false;
    if(ct_len > pt_cap) return false;

    const uint8_t* nonce = blob + OFF_NONCE;
    const uint8_t* ct = blob + OFF_CT;
    const uint8_t* tag = ct + ct_len;
    if(!bv_crypto_gcm_open(key->dek, nonce, ct, ct_len, tag, pt)) return false;

    *pt_len = ct_len;
    return true;
}

// --- Self-test ---

bool bv_vault_selftest(void) {
    BvVaultKey key;
    if(!bv_vault_key_open(&key)) {
        FURI_LOG_E(TAG, "vault selftest: key_open failed");
        return false;
    }

    static const char sample[] =
        "d,u,p\nexample.com,alice,hunter2\nreddit.com,bob,swordfish\n";
    const size_t pt_len = sizeof(sample) - 1; // exclude NUL

    uint8_t blob[128];
    size_t blob_len = 0;
    uint8_t out[128];
    size_t out_len = 0;

    bool ok = (pt_len + BV_BLOB_OVERHEAD <= sizeof(blob)) &&
              bv_vault_encrypt(&key, (const uint8_t*)sample, pt_len, blob, &blob_len) &&
              bv_vault_decrypt(&key, blob, blob_len, out, sizeof(out), &out_len) &&
              (out_len == pt_len) && (memcmp(out, sample, pt_len) == 0);

    FURI_LOG_I(TAG, "vault codec round-trip: %s (blob=%u)", ok ? "PASS" : "FAIL", (unsigned)blob_len);

    bv_vault_key_clear(&key);
    memset(out, 0, sizeof(out));
    return ok;
}
