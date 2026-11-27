#include "flipbip_file.h"
#include <string.h>
#include <ctype.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include "../helpers/flipbip_string.h"
// From: lib/crypto
#include <memzero.h>
#include <rand.h>
#include <bip39.h>

#define TAG "FlipBIP"

#define FLIPBIP_APP_BASE_FOLDER            EXT_PATH("apps_data/flipbip")
#define FLIPBIP_APP_BASE_FOLDER_PATH(path) FLIPBIP_APP_BASE_FOLDER "/" path
#define FLIPBIP_DAT_PATH                   FLIPBIP_APP_BASE_FOLDER_PATH(".flipbip.dat")
#define FLIPBIP_DAT_PATH_BAK               FLIPBIP_APP_BASE_FOLDER_PATH(".flipbip.dat.bak")
#define FLIPBIP_DAT_PATH_TMP               FLIPBIP_APP_BASE_FOLDER_PATH(".flipbip.dat.tmp")
#define FLIPBIP_KEY_PATH                   FLIPBIP_APP_BASE_FOLDER_PATH(".flipbip.key")
#define FLIPBIP_KEY_PATH_BAK               FLIPBIP_APP_BASE_FOLDER_PATH(".flipbip.key.bak")
#define FLIPBIP_KEY_PATH_TMP               FLIPBIP_APP_BASE_FOLDER_PATH(".flipbip.key.tmp")

#define FILE_MAX_PATH_LEN       48
#define FILE_MAX_QRFILE_CONTENT 160 // header 37 + "monero:" 7 + XMR address 95 + null
#define MNEMONIC_MAX_LEN        255 // callers' buffers are 256 bytes

static const char* TEXT_QRFILE = "Filetype: QRCode\n"
                                 "Version: 0\n"
                                 "Message: "; // 37 chars + 1 null

/* ---------------------------------------------------------------------------
 * Plain text files
 * ------------------------------------------------------------------------- */

// Write `text` and a newline to `path`, replacing any existing content.
static bool write_text(Storage* fs, const char* path, const char* text) {
    File* file = storage_file_alloc(fs);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const size_t len = strlen(text);
        ok = storage_file_write(file, text, len) == len && storage_file_write(file, "\n", 1) == 1;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

// Read the first word (up to whitespace or EOF) of `path`, NUL terminated.
static bool read_text(Storage* fs, const char* path, char* out, size_t cap) {
    File* file = storage_file_alloc(fs);
    size_t i = 0;
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char chr;
        while(i < cap - 1 && storage_file_read(file, &chr, 1) == 1 && !isspace((int)chr)) {
            out[i++] = chr;
        }
        ok = i > 0;
    }
    out[i] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

/* ---------------------------------------------------------------------------
 * Legacy format: .flipbip.key + .flipbip.dat, unchanged from earlier versions
 *   key: "fb01" + hex(RC4_k1(k2))       k1 built in, k2 random 128 bytes
 *   dat: "fb01" + hex(RC4_k2(mnemonic))
 * ------------------------------------------------------------------------- */

#define LEGACY_HLEN 4
#define LEGACY_KLEN 256
#define LEGACY_SLEN 512
static const char* LEGACY_HSTR = "fb01";
static const char* LEGACY_K1 = "fb0131d5cf688221c109163908ebe51debb46227c6cc8b37641910833222772a"
                               "baefe6d9ceb651842260e0d1e05e3b90d15e7d5ffaaabc0207bf200a117793a2";

typedef struct {
    char key[LEGACY_HLEN + LEGACY_KLEN + 1];
    char dat[LEGACY_HLEN + LEGACY_SLEN + 1];
} LegacyFiles;

static void legacy_encode(const char* mnemonic, LegacyFiles* f) {
    uint8_t k1[64];
    uint8_t k2[128];
    memzero(f, sizeof(LegacyFiles));
    size_t len = strlen(mnemonic);
    if(len > LEGACY_SLEN / 2) len = LEGACY_SLEN / 2;

    flipbip_xtob(LEGACY_K1, k1, sizeof(k1));
    random_buffer(k2, sizeof(k2));

    memcpy(f->key, LEGACY_HSTR, LEGACY_HLEN);
    flipbip_btox(k2, sizeof(k2), f->key + LEGACY_HLEN);
    flipbip_cipher(k1, sizeof(k1), f->key + LEGACY_HLEN, f->key + LEGACY_HLEN, LEGACY_KLEN);

    memcpy(f->dat, LEGACY_HSTR, LEGACY_HLEN);
    flipbip_btox((const uint8_t*)mnemonic, len, f->dat + LEGACY_HLEN);
    flipbip_cipher(k2, sizeof(k2), f->dat + LEGACY_HLEN, f->dat + LEGACY_HLEN, LEGACY_SLEN);

    memzero(k1, sizeof(k1));
    memzero(k2, sizeof(k2));
}

static bool legacy_decode(LegacyFiles* f, char* mnemonic) {
    if(strncmp(f->key, LEGACY_HSTR, LEGACY_HLEN) != 0) return false;
    if(strncmp(f->dat, LEGACY_HSTR, LEGACY_HLEN) != 0) return false;
    uint8_t k1[64];
    uint8_t k2[128];
    flipbip_xtob(LEGACY_K1, k1, sizeof(k1));
    flipbip_cipher(k1, sizeof(k1), f->key + LEGACY_HLEN, f->key + LEGACY_HLEN, LEGACY_KLEN);
    flipbip_xtob(f->key + LEGACY_HLEN, k2, sizeof(k2));

    char* payload = f->dat + LEGACY_HLEN;
    flipbip_cipher(k2, sizeof(k2), payload, payload, LEGACY_SLEN);
    flipbip_xtob(payload, (unsigned char*)payload, LEGACY_SLEN / 2);
    payload[MNEMONIC_MAX_LEN] = '\0';
    strcpy(mnemonic, payload);

    memzero(k1, sizeof(k1));
    memzero(k2, sizeof(k2));
    return true;
}

static bool legacy_read(Storage* fs, const char* key_path, const char* dat_path, char* mnemonic) {
    LegacyFiles* f = malloc(sizeof(LegacyFiles));
    memzero(f, sizeof(LegacyFiles));
    const bool ok = read_text(fs, key_path, f->key, sizeof(f->key)) &&
                    read_text(fs, dat_path, f->dat, sizeof(f->dat)) && legacy_decode(f, mnemonic);
    memzero(f, sizeof(LegacyFiles));
    free(f);
    return ok;
}

/* ---------------------------------------------------------------------------
 * Enclave format: .flipbip.dat as a FlipperFormat file (as in Astrrra/FlipBIP)
 *   IV:          16 random bytes
 *   Data length: 512
 *   Data:        AES-256-CBC(enclave slot 11, IV, mnemonic zero padded to 512)
 * ------------------------------------------------------------------------- */

#define ENCLAVE_FILETYPE "FlipBIP encrypted seed file version"
#define ENCLAVE_VERSION  1
#define ENCLAVE_DATA_LEN 512
#define ENCLAVE_SLOT     FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT

typedef struct {
    uint8_t iv[16];
    uint8_t data[ENCLAVE_DATA_LEN];
} EnclaveFile;

static bool enclave_crypt(const uint8_t* iv, const uint8_t* in, uint8_t* out, bool encrypt) {
    if(!furi_hal_crypto_enclave_load_key(ENCLAVE_SLOT, iv)) {
        FURI_LOG_E(TAG, "Unable to load enclave key");
        return false;
    }
    const bool ok = encrypt ? furi_hal_crypto_encrypt(in, out, ENCLAVE_DATA_LEN) :
                              furi_hal_crypto_decrypt(in, out, ENCLAVE_DATA_LEN);
    furi_hal_crypto_enclave_unload_key(ENCLAVE_SLOT);
    return ok;
}

static bool enclave_encode(const char* mnemonic, EnclaveFile* f) {
    // Creates this Flipper's unique key on first use (fails on clones)
    if(!furi_hal_crypto_enclave_ensure_key(ENCLAVE_SLOT)) {
        FURI_LOG_E(TAG, "Secure enclave unavailable");
        return false;
    }
    uint8_t* plain = malloc(ENCLAVE_DATA_LEN);
    memzero(plain, ENCLAVE_DATA_LEN);
    strncpy((char*)plain, mnemonic, MNEMONIC_MAX_LEN);
    furi_hal_random_fill_buf(f->iv, sizeof(f->iv));
    const bool ok = enclave_crypt(f->iv, plain, f->data, true);
    memzero(plain, ENCLAVE_DATA_LEN);
    free(plain);
    return ok;
}

static bool enclave_write(Storage* fs, const char* path, const EnclaveFile* f) {
    FlipperFormat* ff = flipper_format_file_alloc(fs);
    uint32_t len = ENCLAVE_DATA_LEN;
    bool ok = flipper_format_file_open_always(ff, path) &&
              flipper_format_write_header_cstr(ff, ENCLAVE_FILETYPE, ENCLAVE_VERSION) &&
              flipper_format_write_hex(ff, "IV", f->iv, sizeof(f->iv)) &&
              flipper_format_write_uint32(ff, "Data length", &len, 1) &&
              flipper_format_write_hex(ff, "Data", f->data, ENCLAVE_DATA_LEN);
    flipper_format_free(ff);
    return ok;
}

// Open `path` and check it is an enclave wallet file. Leaves `ff` positioned
// after the header.
static bool enclave_open(FlipperFormat* ff, const char* path) {
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;
    const bool ok = flipper_format_file_open_existing(ff, path) &&
                    flipper_format_read_header(ff, filetype, &version) &&
                    furi_string_cmp_str(filetype, ENCLAVE_FILETYPE) == 0 &&
                    version == ENCLAVE_VERSION;
    furi_string_free(filetype);
    return ok;
}

static bool is_enclave_file(Storage* fs, const char* path) {
    FlipperFormat* ff = flipper_format_file_alloc(fs);
    const bool ok = enclave_open(ff, path);
    flipper_format_free(ff);
    return ok;
}

static bool enclave_read(Storage* fs, const char* path, char* mnemonic) {
    FlipperFormat* ff = flipper_format_file_alloc(fs);
    EnclaveFile* f = malloc(sizeof(EnclaveFile));
    uint8_t* plain = malloc(ENCLAVE_DATA_LEN);
    uint32_t len = 0;
    bool ok = enclave_open(ff, path) && flipper_format_read_hex(ff, "IV", f->iv, sizeof(f->iv)) &&
              flipper_format_read_uint32(ff, "Data length", &len, 1) && len == ENCLAVE_DATA_LEN &&
              flipper_format_read_hex(ff, "Data", f->data, ENCLAVE_DATA_LEN) &&
              enclave_crypt(f->iv, f->data, plain, false);
    if(ok) {
        plain[MNEMONIC_MAX_LEN] = 0;
        strcpy(mnemonic, (const char*)plain);
    }
    flipper_format_free(ff);
    memzero(plain, ENCLAVE_DATA_LEN);
    free(plain);
    memzero(f, sizeof(EnclaveFile));
    free(f);
    return ok;
}

/* ---------------------------------------------------------------------------
 * Wallet files
 * ------------------------------------------------------------------------- */

bool flipbip_enclave_available(void) {
    uint8_t keys = 0, valid = 0;
    return furi_hal_crypto_enclave_verify(&keys, &valid);
}

static FlipBipWalletFormat wallet_format(Storage* fs) {
    if(!storage_file_exists(fs, FLIPBIP_DAT_PATH)) return FlipBipWalletNone;
    if(is_enclave_file(fs, FLIPBIP_DAT_PATH)) return FlipBipWalletEnclave;
    if(storage_file_exists(fs, FLIPBIP_KEY_PATH)) return FlipBipWalletLegacy;
    return FlipBipWalletNone;
}

FlipBipWalletFormat flipbip_wallet_format(void) {
    Storage* fs = furi_record_open(RECORD_STORAGE);
    const FlipBipWalletFormat format = wallet_format(fs);
    furi_record_close(RECORD_STORAGE);
    return format;
}

static bool valid_mnemonic(const char* mnemonic) {
    return mnemonic[0] != '\0' && mnemonic_check(mnemonic) != 0;
}

// Read a wallet (main files or backups) in whichever format it is in
static bool read_wallet(Storage* fs, const char* key_path, const char* dat_path, char* mnemonic) {
    memzero(mnemonic, MNEMONIC_MAX_LEN + 1);
    const bool ok = is_enclave_file(fs, dat_path) ? enclave_read(fs, dat_path, mnemonic) :
                                                    legacy_read(fs, key_path, dat_path, mnemonic);
    if(ok && valid_mnemonic(mnemonic)) return true;
    memzero(mnemonic, MNEMONIC_MAX_LEN + 1);
    return false;
}

bool flipbip_load_file_secure(char* mnemonic) {
    Storage* fs = furi_record_open(RECORD_STORAGE);
    bool ok = read_wallet(fs, FLIPBIP_KEY_PATH, FLIPBIP_DAT_PATH, mnemonic);
    if(!ok) {
        FURI_LOG_W(TAG, "Wallet files unreadable, trying backups");
        ok = read_wallet(fs, FLIPBIP_KEY_PATH_BAK, FLIPBIP_DAT_PATH_BAK, mnemonic);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// Rename a checked temporary file over `path` (storage_common_rename replaces it)
static bool commit(Storage* fs, const char* tmp, const char* path) {
    return storage_common_rename(fs, tmp, path) == FSE_OK;
}

static bool save_enclave(Storage* fs, const char* mnemonic, char* check) {
    EnclaveFile* f = malloc(sizeof(EnclaveFile));
    bool ok = enclave_encode(mnemonic, f) && enclave_write(fs, FLIPBIP_DAT_PATH_TMP, f) &&
              enclave_read(fs, FLIPBIP_DAT_PATH_TMP, check) && strcmp(check, mnemonic) == 0 &&
              commit(fs, FLIPBIP_DAT_PATH_TMP, FLIPBIP_DAT_PATH);
    if(ok) {
        // Same IV and ciphertext as backup; then remove the legacy key files,
        // which would otherwise still decrypt a legacy .dat.bak
        if(enclave_write(fs, FLIPBIP_DAT_PATH_TMP, f)) {
            commit(fs, FLIPBIP_DAT_PATH_TMP, FLIPBIP_DAT_PATH_BAK);
        }
        storage_simply_remove(fs, FLIPBIP_KEY_PATH);
        storage_simply_remove(fs, FLIPBIP_KEY_PATH_BAK);
    }
    memzero(f, sizeof(EnclaveFile));
    free(f);
    return ok;
}

static bool save_legacy(Storage* fs, const char* mnemonic, char* check) {
    LegacyFiles* f = malloc(sizeof(LegacyFiles));
    legacy_encode(mnemonic, f);
    bool ok = write_text(fs, FLIPBIP_KEY_PATH_TMP, f->key) &&
              write_text(fs, FLIPBIP_DAT_PATH_TMP, f->dat) &&
              legacy_read(fs, FLIPBIP_KEY_PATH_TMP, FLIPBIP_DAT_PATH_TMP, check) &&
              strcmp(check, mnemonic) == 0 && commit(fs, FLIPBIP_KEY_PATH_TMP, FLIPBIP_KEY_PATH) &&
              commit(fs, FLIPBIP_DAT_PATH_TMP, FLIPBIP_DAT_PATH);
    if(ok) {
        if(write_text(fs, FLIPBIP_KEY_PATH_TMP, f->key)) {
            commit(fs, FLIPBIP_KEY_PATH_TMP, FLIPBIP_KEY_PATH_BAK);
        }
        if(write_text(fs, FLIPBIP_DAT_PATH_TMP, f->dat)) {
            commit(fs, FLIPBIP_DAT_PATH_TMP, FLIPBIP_DAT_PATH_BAK);
        }
    }
    memzero(f, sizeof(LegacyFiles));
    free(f);
    return ok;
}

bool flipbip_save_file_secure(const char* mnemonic, FlipBipEncryption encryption) {
    if(!valid_mnemonic(mnemonic)) return false;
    Storage* fs = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(fs, FLIPBIP_APP_BASE_FOLDER);
    char* check = malloc(MNEMONIC_MAX_LEN + 1);
    memzero(check, MNEMONIC_MAX_LEN + 1);
    const bool ok = encryption == FlipBipEncryptionEnclave ? save_enclave(fs, mnemonic, check) :
                                                             save_legacy(fs, mnemonic, check);
    // Nothing secret may be left in temporary files after a failure
    storage_simply_remove(fs, FLIPBIP_KEY_PATH_TMP);
    storage_simply_remove(fs, FLIPBIP_DAT_PATH_TMP);
    memzero(check, MNEMONIC_MAX_LEN + 1);
    free(check);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool flipbip_switch_encryption(FlipBipEncryption encryption) {
    const FlipBipWalletFormat format = flipbip_wallet_format();
    if(format == FlipBipWalletNone) return true;
    if((format == FlipBipWalletEnclave) == (encryption == FlipBipEncryptionEnclave)) return true;
    char* mnemonic = malloc(MNEMONIC_MAX_LEN + 1);
    const bool ok = flipbip_load_file_secure(mnemonic) &&
                    flipbip_save_file_secure(mnemonic, encryption);
    memzero(mnemonic, MNEMONIC_MAX_LEN + 1);
    free(mnemonic);
    return ok;
}

void flipbip_wallet_recover(void) {
    Storage* fs = furi_record_open(RECORD_STORAGE);
    // A switch that stopped before its commit: the old wallet is still in place
    storage_simply_remove(fs, FLIPBIP_KEY_PATH_TMP);
    storage_simply_remove(fs, FLIPBIP_DAT_PATH_TMP);
    // A switch to the enclave that stopped after its commit: finish it, so no
    // legacy (readable on any Flipper) copy of the mnemonic is left behind
    const bool leftovers = storage_file_exists(fs, FLIPBIP_KEY_PATH) ||
                           storage_file_exists(fs, FLIPBIP_KEY_PATH_BAK);
    const bool enclave = wallet_format(fs) == FlipBipWalletEnclave;
    furi_record_close(RECORD_STORAGE);
    if(enclave && leftovers && flipbip_enclave_available()) {
        char* mnemonic = malloc(MNEMONIC_MAX_LEN + 1);
        if(flipbip_load_file_secure(mnemonic)) {
            flipbip_save_file_secure(mnemonic, FlipBipEncryptionEnclave);
        }
        memzero(mnemonic, MNEMONIC_MAX_LEN + 1);
        free(mnemonic);
    }
}

/* ---------------------------------------------------------------------------
 * QR code files for receive addresses
 * ------------------------------------------------------------------------- */

bool flipbip_save_qrfile(
    const char* qr_msg_prefix,
    const char* qr_msg_content,
    const char* file_name) {
    char path[FILE_MAX_PATH_LEN];
    char qr_buf[FILE_MAX_QRFILE_CONTENT];
    snprintf(path, sizeof(path), "%s/%s", FLIPBIP_APP_BASE_FOLDER, file_name);
    snprintf(qr_buf, sizeof(qr_buf), "%s%s%s", TEXT_QRFILE, qr_msg_prefix, qr_msg_content);
    Storage* fs = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(fs, FLIPBIP_APP_BASE_FOLDER);
    const bool ok = write_text(fs, path, qr_buf);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
