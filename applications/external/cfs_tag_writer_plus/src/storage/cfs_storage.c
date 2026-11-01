#include "src/storage/cfs_storage.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

#include "src/crypto/cfs_crypto.h"

#include <nfc/nfc_device.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#define TAG "CfsStorage"

// Saved tags are official Flipper NFC MIFARE Classic 1K dumps (.nfc), serialized
// via the SDK's NfcDevice/MfClassic writer (not a hand-rolled text format). The
// CFS payload lives in sector 1 (blocks 4/5/6, ciphertext); block 7 is the
// trailer. We synthesize a full 64-block dump on save; on load we only trust
// blocks 4/5/6 and rebuild everything else at write time.
#define CFS_MC_BLOCKS             (64)
#define CFS_MC_BLOCK_SIZE         (16)
#define CFS_STORAGE_PATH_BUF_SIZE (CFS_STORAGE_NAME_MAX + sizeof(CFS_STORAGE_DIR) + 8)

// cfs_access_bits (sector-1 trailer) is shared from cfs_crypto.

void cfs_storage_ensure_dir(Storage* storage) {
    furi_assert(storage);
    storage_simply_mkdir(storage, CFS_STORAGE_DIR);
}

#define CFS_ORIGINAL_DIR      EXT_PATH("apps_data/cfs_tag_writer")
#define CFS_ORIGINAL_TAGS     CFS_ORIGINAL_DIR "/tags"
#define CFS_ORIGINAL_SETTINGS CFS_ORIGINAL_DIR "/settings"

void cfs_storage_import_original(Storage* storage) {
    furi_assert(storage);
    // This app's tags folder only goes missing before the very first run.
    if(storage_dir_exists(storage, CFS_STORAGE_DIR)) return;
    storage_simply_mkdir(storage, CFS_STORAGE_DIR);

    if(storage_file_exists(storage, CFS_ORIGINAL_SETTINGS) &&
       !storage_file_exists(storage, APP_DATA_PATH("settings"))) {
        storage_common_copy(storage, CFS_ORIGINAL_SETTINGS, APP_DATA_PATH("settings"));
    }

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, CFS_ORIGINAL_TAGS)) {
        FileInfo info;
        char name[CFS_STORAGE_NAME_MAX + 8];
        FuriString* src = furi_string_alloc();
        FuriString* dst = furi_string_alloc();
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            furi_string_printf(src, "%s/%s", CFS_ORIGINAL_TAGS, name);
            furi_string_printf(dst, "%s/%s", CFS_STORAGE_DIR, name);
            storage_common_copy(storage, furi_string_get_cstr(src), furi_string_get_cstr(dst));
        }
        furi_string_free(src);
        furi_string_free(dst);
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

// Build the on-disk path for a saved dump from its base name (adds the dir and
// extension). Shared by save and the exists-check so the layout lives in one place.
void cfs_storage_path_in(const char* dir, const char* name, char* out, size_t out_size) {
    snprintf(out, out_size, "%s/%s%s", dir, name, CFS_STORAGE_EXTENSION);
}

// ---- Save ----

// Fill the 16 bytes of MIFARE Classic 1K block `idx` for a synthesized dump.
// Sector-1 data blocks (4/5/6) get the ciphertext; block 7 gets the derived
// sector keys + access bits; every other trailer gets factory-default keys;
// all remaining data blocks are zeroed.
static void cfs_fill_block(
    int idx,
    uint8_t out[CFS_MC_BLOCK_SIZE],
    const uint8_t* uid,
    size_t uid_len,
    const uint8_t cipher[CFS_PLAINTEXT_SIZE],
    const MfClassicKey* key_a,
    const MfClassicKey* key_b) {
    memset(out, 0, CFS_MC_BLOCK_SIZE);

    if(idx == 0) {
        // Manufacturer block: UID(4) BCC SAK ATQA(2) + 8 manufacturer bytes.
        uint8_t u[4] = {0};
        size_t n = (uid_len < 4) ? uid_len : 4;
        if(uid && n) memcpy(u, uid, n);
        memcpy(out, u, 4);
        out[4] = u[0] ^ u[1] ^ u[2] ^ u[3]; // BCC
        out[5] = 0x08; // SAK
        out[6] = 0x04; // ATQA low
        out[7] = 0x00; // ATQA high
        return;
    }
    if(idx >= 4 && idx <= 6) {
        memcpy(out, cipher + (idx - 4) * CFS_MC_BLOCK_SIZE, CFS_MC_BLOCK_SIZE);
        return;
    }
    if((idx % 4) == 3) {
        // Sector trailer: KeyA(6) + access(3) + GPB(1) + KeyB(6).
        if(idx == 7) {
            memcpy(out, key_a->data, 6);
            memcpy(out + 10, key_b->data, 6);
        } else {
            memset(out, 0xFF, 6);
            memset(out + 10, 0xFF, 6);
        }
        memcpy(out + 6, cfs_access_bits, 4);
        return;
    }
    // Remaining data blocks stay zero.
}

bool cfs_storage_save(
    Storage* storage,
    const uint8_t plaintext[CFS_PLAINTEXT_SIZE],
    const uint8_t* uid,
    size_t uid_len,
    const char* name) {
    return cfs_storage_save_in(storage, CFS_STORAGE_DIR, plaintext, uid, uid_len, name);
}

bool cfs_storage_save_in(
    Storage* storage,
    const char* dir,
    const uint8_t plaintext[CFS_PLAINTEXT_SIZE],
    const uint8_t* uid,
    size_t uid_len,
    const char* name) {
    furi_assert(storage);
    furi_assert(plaintext);

    storage_simply_mkdir(storage, dir);

    uint8_t cipher[CFS_PLAINTEXT_SIZE];
    cfs_crypto_encrypt(plaintext, cipher);

    // Sector-1 keys for block 7. When we don't have a real UID (saving a staged
    // write), fall back to factory-default keys — block 7 is rebuilt from the
    // target tag's own UID at write time regardless.
    MfClassicKey key_a, key_b;
    if(uid && uid_len) {
        cfs_crypto_derive_key_a(uid, uid_len, &key_a);
        cfs_crypto_derive_key_b(uid, uid_len, &key_b);
    } else {
        memset(key_a.data, 0xFF, sizeof(key_a.data));
        memset(key_b.data, 0xFF, sizeof(key_b.data));
    }

    // 4-byte UID (zero placeholder for a staged write with no tag).
    uint8_t u[4] = {0};
    size_t un = (uid_len < 4) ? uid_len : 4;
    if(uid && un) memcpy(u, uid, un);

    MfClassicData* data = mf_classic_alloc();
    data->type = MfClassicType1k;

    Iso14443_3aData* iso = mf_classic_get_base_data(data);
    iso14443_3a_set_uid(iso, u, sizeof(u));
    const uint8_t atqa[2] = {0x04, 0x00};
    iso14443_3a_set_atqa(iso, atqa);
    iso14443_3a_set_sak(iso, 0x08);

    for(int i = 0; i < CFS_MC_BLOCKS; i++) {
        MfClassicBlock block;
        cfs_fill_block(i, block.data, u, sizeof(u), cipher, &key_a, &key_b);
        mf_classic_set_block_read(data, i, &block);
    }

    char path[CFS_STORAGE_PATH_BUF_SIZE];
    cfs_storage_path_in(dir, (name && name[0]) ? name : "tag", path, sizeof(path));

    NfcDevice* dev = nfc_device_alloc();
    nfc_device_set_data(dev, NfcProtocolMfClassic, data); // copies data
    bool ok = nfc_device_save(dev, path);
    nfc_device_free(dev);
    mf_classic_free(data);

    if(!ok) FURI_LOG_E(TAG, "Failed to save %s", path);
    return ok;
}

bool cfs_storage_exists(Storage* storage, const char* name) {
    return cfs_storage_exists_in(storage, CFS_STORAGE_DIR, name);
}

bool cfs_storage_exists_in(Storage* storage, const char* dir, const char* name) {
    furi_assert(storage);
    if(!name || !name[0]) return false;
    char path[CFS_STORAGE_PATH_BUF_SIZE];
    cfs_storage_path_in(dir, name, path, sizeof(path));
    return storage_file_exists(storage, path);
}

size_t cfs_storage_list(
    Storage* storage,
    const char* dir,
    char (*names)[CFS_STORAGE_NAME_MAX],
    size_t max) {
    furi_assert(storage);
    size_t count = 0;
    const size_t ext_len = strlen(CFS_STORAGE_EXTENSION);
    File* file = storage_file_alloc(storage);
    if(storage_dir_open(file, dir)) {
        FileInfo info;
        char name[CFS_STORAGE_NAME_MAX + 8];
        while(count < max && storage_dir_read(file, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(name);
            if(len <= ext_len || strcmp(name + len - ext_len, CFS_STORAGE_EXTENSION) != 0)
                continue;
            len -= ext_len;
            if(len >= CFS_STORAGE_NAME_MAX) continue; // longer than we ever save
            memcpy(names[count], name, len);
            names[count][len] = '\0';
            count++;
        }
    }
    storage_dir_close(file);
    storage_file_free(file);
    // Insertion sort, A-Z (qsort isn't exported to apps; lists are short).
    char tmp[CFS_STORAGE_NAME_MAX];
    for(size_t i = 1; i < count; i++) {
        memcpy(tmp, names[i], CFS_STORAGE_NAME_MAX);
        size_t j = i;
        while(j > 0 && strcmp(names[j - 1], tmp) > 0) {
            memcpy(names[j], names[j - 1], CFS_STORAGE_NAME_MAX);
            j--;
        }
        memcpy(names[j], tmp, CFS_STORAGE_NAME_MAX);
    }
    return count;
}

// ---- Load ----

bool cfs_storage_load(Storage* storage, const char* path, CfsLoadedTag* out) {
    furi_assert(path);
    furi_assert(out);
    UNUSED(storage); // NfcDevice manages its own storage record

    NfcDevice* dev = nfc_device_alloc();
    bool ok = false;

    if(nfc_device_load(dev, path) && nfc_device_get_protocol(dev) == NfcProtocolMfClassic) {
        const MfClassicData* data = nfc_device_get_data(dev, NfcProtocolMfClassic);
        // Only trust the CFS payload if blocks 4/5/6 were actually captured
        // (a partial dump leaves them unread == the old "??").
        if(mf_classic_is_block_read(data, 4) && mf_classic_is_block_read(data, 5) &&
           mf_classic_is_block_read(data, 6)) {
            uint8_t cipher[CFS_PLAINTEXT_SIZE];
            for(int i = 0; i < 3; i++) {
                memcpy(cipher + i * CFS_MC_BLOCK_SIZE, data->block[4 + i].data, CFS_MC_BLOCK_SIZE);
            }
            cfs_crypto_decrypt(cipher, out->plain);
            if(cfs_data_decode(out->plain, &out->data)) {
                size_t ul = 0;
                const uint8_t* u = mf_classic_get_uid(data, &ul);
                if(ul > sizeof(out->uid)) ul = sizeof(out->uid);
                if(u) memcpy(out->uid, u, ul);
                out->uid_len = ul;
                ok = true;
            }
        }
    }

    nfc_device_free(dev);
    if(!ok) FURI_LOG_E(TAG, "Failed to load %s", path);
    return ok;
}

bool cfs_storage_delete(Storage* storage, const char* path) {
    furi_assert(storage);
    furi_assert(path);
    return storage_common_remove(storage, path) == FSE_OK;
}
