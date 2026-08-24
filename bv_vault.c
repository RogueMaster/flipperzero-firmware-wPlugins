#include "bv_vault.h"
#include "bv_pin.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <string.h>
#include <toolbox/compress.h>

#define TAG "BioVaultVault"

// Absolute path, NOT APP_DATA_PATH: /data resolves per calling thread, so the
// CLI shell thread (owner app "cli_vcp") would silently read/write a keystore
// in the wrong apps_data directory.
#define BV_DATA_DIR EXT_PATH("apps_data/biovault")
#define KEYSTORE_PATH BV_DATA_DIR "/keystore.bin"
#define KS_MAGIC "BVK1"
#define KS_MAGIC_V2 "BVK2"
#define KS_MAGIC_LEN 4
#define KS_SIZE (KS_MAGIC_LEN + BV_WRAP_IV_SIZE + BV_DEK_SIZE) // 52
// v2 layout offsets: [magic:4][salt:16][sw_iters:4][hw_iters:4][wrap_iv:16][wrapped:32]
#define KS2_OFF_SALT KS_MAGIC_LEN
#define KS2_OFF_SW (KS2_OFF_SALT + BV_PIN_SALT_SIZE)
#define KS2_OFF_HW (KS2_OFF_SW + 4)
#define KS2_OFF_IV (KS2_OFF_HW + 4)
#define KS2_OFF_WRAPPED (KS2_OFF_IV + BV_WRAP_IV_SIZE)
#define KS2_SIZE (KS2_OFF_WRAPPED + BV_DEK_SIZE) // 76

// Session unlock key (bv_pin_derive output), RAM only.
static uint8_t s_unlock_key[BV_PIN_KEY_SIZE];
static bool s_unlock_set = false;

// On-tag blob framing.
#define BLOB_MAGIC0 'B'
#define BLOB_MAGIC1 'V'
#define BLOB_VER_RAW 1 // plaintext = raw serialized records (legacy, read-only)
#define BLOB_VER_LZ 2 // plaintext = heatshrink stream (toolbox compress framing)
#define OFF_NONCE 3
#define OFF_CTLEN (OFF_NONCE + BV_GCM_IV_SIZE) // 15
#define OFF_CT BV_BLOB_HEADER // 17

// --- Keystore file I/O ---

typedef enum {
    KsReadOk,
    KsReadMissing, // no keystore file
    KsReadBad, // exists but unreadable/short; must not re-key
} KsReadStatus;

// Read the keystore (any version) into `buf` (cap bytes); `out_len` gets the
// byte count actually read.
static KsReadStatus ks_read(uint8_t* buf, size_t cap, size_t* out_len) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    KsReadStatus status;
    if(storage_file_open(file, KEYSTORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        *out_len = storage_file_read(file, buf, cap);
        status = (*out_len >= KS_MAGIC_LEN) ? KsReadOk : KsReadBad;
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
    storage_common_mkdir(storage, BV_DATA_DIR);
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

    uint8_t ks[KS2_SIZE];
    size_t ks_len = 0;
    KsReadStatus status = ks_read(ks, sizeof(ks), &ks_len);

    if(status == KsReadOk && ks_len == KS_SIZE && memcmp(ks, KS_MAGIC, KS_MAGIC_LEN) == 0) {
        // v1: plain enclave wrap.
        bool ok = bv_crypto_kek_unwrap(ks + KS_MAGIC_LEN, ks + KS_MAGIC_LEN + BV_WRAP_IV_SIZE,
            out->dek);
        memset(ks, 0, sizeof(ks));
        if(!ok) FURI_LOG_E(TAG, "DEK unwrap failed (enclave key changed?)");
        return ok;
    }
    if(status == KsReadOk && ks_len == KS2_SIZE && memcmp(ks, KS_MAGIC_V2, KS_MAGIC_LEN) == 0) {
        // v2: enclave wrap over (DEK XOR unlock_key). No verifier: a wrong
        // unlock key yields a garbage DEK; only the vault's GCM tag can tell.
        if(!s_unlock_set) {
            memset(ks, 0, sizeof(ks));
            FURI_LOG_E(TAG, "keystore needs PIN but no unlock key set");
            return false;
        }
        bool ok = bv_crypto_kek_unwrap(ks + KS2_OFF_IV, ks + KS2_OFF_WRAPPED, out->dek);
        memset(ks, 0, sizeof(ks));
        if(!ok) {
            FURI_LOG_E(TAG, "DEK unwrap failed (enclave key changed?)");
            return false;
        }
        for(size_t i = 0; i < BV_DEK_SIZE; i++) out->dek[i] ^= s_unlock_key[i];
        return true;
    }
    if(status != KsReadMissing) {
        // Refuse to re-key: would orphan the on-tag vault.
        FURI_LOG_E(TAG, "keystore present but unreadable/corrupt; refusing to re-key");
        memset(ks, 0, sizeof(ks));
        return false;
    }

    // First use: generate a fresh DEK, wrap it, persist (v1; PIN is opt-in).
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
    bool ok = ks_write(ks, KS_SIZE);
    memset(ks, 0, sizeof(ks));
    memset(wrapped, 0, sizeof(wrapped));
    FURI_LOG_I(TAG, "created new keystore: %s", ok ? "OK" : "FAIL");
    return ok;
}

// --- PIN (unlock key) management ---

bool bv_vault_pin_required(void) {
    uint8_t ks[KS2_SIZE];
    size_t ks_len = 0;
    bool v2 = (ks_read(ks, sizeof(ks), &ks_len) == KsReadOk) && (ks_len == KS2_SIZE) &&
              (memcmp(ks, KS_MAGIC_V2, KS_MAGIC_LEN) == 0);
    memset(ks, 0, sizeof(ks));
    return v2;
}

bool bv_vault_pin_params(uint8_t salt[16], uint32_t* sw_iters, uint32_t* hw_iters) {
    uint8_t ks[KS2_SIZE];
    size_t ks_len = 0;
    bool v2 = (ks_read(ks, sizeof(ks), &ks_len) == KsReadOk) && (ks_len == KS2_SIZE) &&
              (memcmp(ks, KS_MAGIC_V2, KS_MAGIC_LEN) == 0);
    if(v2) {
        memcpy(salt, ks + KS2_OFF_SALT, BV_PIN_SALT_SIZE);
        *sw_iters = (uint32_t)ks[KS2_OFF_SW] | ((uint32_t)ks[KS2_OFF_SW + 1] << 8) |
                    ((uint32_t)ks[KS2_OFF_SW + 2] << 16) | ((uint32_t)ks[KS2_OFF_SW + 3] << 24);
        *hw_iters = (uint32_t)ks[KS2_OFF_HW] | ((uint32_t)ks[KS2_OFF_HW + 1] << 8) |
                    ((uint32_t)ks[KS2_OFF_HW + 2] << 16) | ((uint32_t)ks[KS2_OFF_HW + 3] << 24);
    }
    memset(ks, 0, sizeof(ks));
    return v2;
}

void bv_vault_unlock_key_set(const uint8_t key[32]) {
    memcpy(s_unlock_key, key, BV_PIN_KEY_SIZE);
    s_unlock_set = true;
}

void bv_vault_unlock_key_clear(void) {
    memset(s_unlock_key, 0, sizeof(s_unlock_key));
    s_unlock_set = false;
}

bool bv_vault_pin_enable(const char* pin) {
    // Unwrap with the current session state; the caller has proven it correct.
    BvVaultKey key;
    if(!bv_vault_key_open(&key)) return false;

    uint8_t salt[BV_PIN_SALT_SIZE];
    uint8_t unlock[BV_PIN_KEY_SIZE];
    furi_hal_random_fill_buf(salt, sizeof(salt));
    bool ok = bv_pin_derive(pin, salt, BV_PIN_SW_ITERS, BV_PIN_HW_ITERS, unlock);

    uint8_t ks[KS2_SIZE];
    if(ok) {
        uint8_t masked[BV_DEK_SIZE];
        uint8_t wrap_iv[BV_WRAP_IV_SIZE];
        uint8_t wrapped[BV_DEK_SIZE];
        for(size_t i = 0; i < BV_DEK_SIZE; i++) masked[i] = key.dek[i] ^ unlock[i];
        furi_hal_random_fill_buf(wrap_iv, sizeof(wrap_iv));
        ok = bv_crypto_kek_wrap(wrap_iv, masked, wrapped);
        if(ok) {
            memcpy(ks, KS_MAGIC_V2, KS_MAGIC_LEN);
            memcpy(ks + KS2_OFF_SALT, salt, BV_PIN_SALT_SIZE);
            for(int i = 0; i < 4; i++) ks[KS2_OFF_SW + i] = (uint8_t)(BV_PIN_SW_ITERS >> (8 * i));
            for(int i = 0; i < 4; i++) ks[KS2_OFF_HW + i] = (uint8_t)(BV_PIN_HW_ITERS >> (8 * i));
            memcpy(ks + KS2_OFF_IV, wrap_iv, BV_WRAP_IV_SIZE);
            memcpy(ks + KS2_OFF_WRAPPED, wrapped, BV_DEK_SIZE);
            ok = ks_write(ks, KS2_SIZE);
        }
        memset(masked, 0, sizeof(masked));
        memset(wrapped, 0, sizeof(wrapped));
    }
    if(ok) bv_vault_unlock_key_set(unlock);
    memset(ks, 0, sizeof(ks));
    memset(unlock, 0, sizeof(unlock));
    bv_vault_key_clear(&key);
    FURI_LOG_I(TAG, "PIN enable: %s", ok ? "OK" : "FAIL");
    return ok;
}

bool bv_vault_pin_disable(void) {
    BvVaultKey key;
    if(!bv_vault_key_open(&key)) return false;

    uint8_t ks[KS_SIZE];
    uint8_t wrap_iv[BV_WRAP_IV_SIZE];
    uint8_t wrapped[BV_DEK_SIZE];
    furi_hal_random_fill_buf(wrap_iv, sizeof(wrap_iv));
    bool ok = bv_crypto_kek_wrap(wrap_iv, key.dek, wrapped);
    if(ok) {
        memcpy(ks, KS_MAGIC, KS_MAGIC_LEN);
        memcpy(ks + KS_MAGIC_LEN, wrap_iv, BV_WRAP_IV_SIZE);
        memcpy(ks + KS_MAGIC_LEN + BV_WRAP_IV_SIZE, wrapped, BV_DEK_SIZE);
        ok = ks_write(ks, KS_SIZE);
    }
    if(ok) bv_vault_unlock_key_clear();
    memset(ks, 0, sizeof(ks));
    memset(wrapped, 0, sizeof(wrapped));
    bv_vault_key_clear(&key);
    FURI_LOG_I(TAG, "PIN disable: %s", ok ? "OK" : "FAIL");
    return ok;
}

void bv_vault_key_clear(BvVaultKey* key) {
    memset(key, 0, sizeof(*key));
}

void bv_vault_tag_password(
    const BvVaultKey* key,
    const uint8_t* uid,
    size_t uid_len,
    uint8_t pwd[4],
    uint8_t pack[2]) {
    // UID-diversified derivation: GCM-seal domain label + UID, take first 6 CT bytes.
    // Fixed nonce is safe: one-way derivation, never protects stored data.
    static const uint8_t deriv_iv[BV_GCM_IV_SIZE] = {0};
    uint8_t input[16] = {'B', 'V', 'T', 'A', 'G', 'P', 'W', 'D'}; // + UID, zero-padded
    if(uid && uid_len) {
        size_t n = uid_len > 8 ? 8 : uid_len;
        memcpy(input + 8, uid, n);
    }
    uint8_t ct[16] = {0};
    uint8_t tag[BV_GCM_TAG_SIZE] = {0};
    if(bv_crypto_gcm_seal(key->dek, deriv_iv, input, sizeof(input), ct, tag)) {
        memcpy(pwd, ct, 4);
        memcpy(pack, ct + 4, 2);
    } else {
        // Fall back to tag default so a bug can't set an unknown password.
        memset(pwd, 0xFF, 4);
        memset(pack, 0x00, 2);
    }
    memset(ct, 0, sizeof(ct));
}

// --- Vault blob codec ---

bool bv_vault_encrypt(
    const BvVaultKey* key,
    const uint8_t* pt,
    size_t pt_len,
    uint8_t* blob,
    size_t* blob_len) {
    if(pt_len == 0 || pt_len > 0xFFFF) return false;

    // Compress first (v2 blobs). compress_encode stores raw with 1 byte of
    // framing when compression doesn't win, so z_len <= pt_len + 1.
    size_t z_cap = pt_len + 8;
    uint8_t* z = malloc(z_cap);
    size_t z_len = 0;
    Compress* comp = compress_alloc(CompressTypeHeatshrink, &compress_config_heatshrink_default);
    bool z_ok = compress_encode(comp, (uint8_t*)pt, pt_len, z, z_cap, &z_len);
    compress_free(comp);
    if(!z_ok || z_len == 0 || z_len > 0xFFFF) {
        memset(z, 0, z_cap);
        free(z);
        return false;
    }

    uint8_t nonce[BV_GCM_IV_SIZE];
    furi_hal_random_fill_buf(nonce, sizeof(nonce));

    blob[0] = BLOB_MAGIC0;
    blob[1] = BLOB_MAGIC1;
    blob[2] = BLOB_VER_LZ;
    memcpy(blob + OFF_NONCE, nonce, BV_GCM_IV_SIZE);
    blob[OFF_CTLEN] = (uint8_t)(z_len & 0xFF);
    blob[OFF_CTLEN + 1] = (uint8_t)((z_len >> 8) & 0xFF);

    uint8_t tag[BV_GCM_TAG_SIZE];
    bool ok = bv_crypto_gcm_seal(key->dek, nonce, z, z_len, blob + OFF_CT, tag);
    memset(z, 0, z_cap);
    free(z);
    if(!ok) return false;
    memcpy(blob + OFF_CT + z_len, tag, BV_GCM_TAG_SIZE);

    *blob_len = OFF_CT + z_len + BV_GCM_TAG_SIZE;
    return true;
}

bool bv_vault_framed_len(const uint8_t* blob, size_t avail, size_t* out_len) {
    if(avail < BV_BLOB_OVERHEAD) return false;
    if(blob[0] != BLOB_MAGIC0 || blob[1] != BLOB_MAGIC1) return false;
    if(blob[2] != BLOB_VER_RAW && blob[2] != BLOB_VER_LZ) return false;
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
    if(blob[0] != BLOB_MAGIC0 || blob[1] != BLOB_MAGIC1) return false;
    uint8_t ver = blob[2];
    if(ver != BLOB_VER_RAW && ver != BLOB_VER_LZ) return false;

    size_t ct_len = (size_t)blob[OFF_CTLEN] | ((size_t)blob[OFF_CTLEN + 1] << 8);
    if(blob_len != OFF_CT + ct_len + BV_GCM_TAG_SIZE) return false;
    if(ct_len == 0 || ct_len > pt_cap) return false;

    const uint8_t* nonce = blob + OFF_NONCE;
    const uint8_t* ct = blob + OFF_CT;
    const uint8_t* tag = ct + ct_len;

    if(ver == BLOB_VER_RAW) {
        if(!bv_crypto_gcm_open(key->dek, nonce, ct, ct_len, tag, pt)) return false;
        *pt_len = ct_len;
        return true;
    }

    // v2: decrypt the compressed stream, then inflate into pt (bounded by
    // pt_cap; compress_decode fails rather than overrun). +1 slack: the
    // toolbox raw-fallback path touches one byte past the input length.
    uint8_t* z = malloc(ct_len + 1);
    bool ok = bv_crypto_gcm_open(key->dek, nonce, ct, ct_len, tag, z);
    if(ok) {
        Compress* comp =
            compress_alloc(CompressTypeHeatshrink, &compress_config_heatshrink_default);
        ok = compress_decode(comp, z, ct_len, pt, pt_cap, pt_len);
        compress_free(comp);
    }
    memset(z, 0, ct_len + 1);
    free(z);
    return ok;
}

// --- Self-test ---

bool bv_vault_selftest(void) {
    // Throwaway random DEK: keeps the codec test independent of keystore/PIN
    // state (runs at startup, before any PIN is entered).
    BvVaultKey key;
    furi_hal_random_fill_buf(key.dek, BV_DEK_SIZE);

    static const char sample[] =
        "d,u,p\nexample.com,alice,hunter2\nreddit.com,bob,swordfish\n";
    const size_t pt_len = sizeof(sample) - 1;

    uint8_t blob[128];
    size_t blob_len = 0;
    uint8_t out[128];
    size_t out_len = 0;

    bool ok = (pt_len + 1 + BV_BLOB_OVERHEAD <= sizeof(blob)) &&
              bv_vault_encrypt(&key, (const uint8_t*)sample, pt_len, blob, &blob_len) &&
              bv_vault_decrypt(&key, blob, blob_len, out, sizeof(out), &out_len) &&
              (out_len == pt_len) && (memcmp(out, sample, pt_len) == 0);

    FURI_LOG_I(TAG, "vault codec round-trip: %s (blob=%u)", ok ? "PASS" : "FAIL", (unsigned)blob_len);

    bv_vault_key_clear(&key);
    memset(out, 0, sizeof(out));
    return ok;
}
