#include "mfc_key_cache.h"

#include <furi/furi.h>
#include <flipper_format/flipper_format.h>

#define TAG "MfcKeyCache"

#define MFC_KEY_CACHE_FOLDER    "/ext/nfc/.cache"
#define MFC_KEY_CACHE_EXTENSION ".keys"

// Must match applications/main/nfc/helpers/mf_classic_key_cache.c in the firmware.
static const char* mfc_key_cache_file_header = "Flipper NFC keys";
static const uint32_t mfc_key_cache_file_version = 1;

// The firmware's own cache holder wraps this same SDK type. A map bit below
// MF_CLASSIC_TOTAL_SECTORS_MAX set = that sector's key is loaded; the file's maps are 64-bit, so
// any higher bit addresses no key slot and is never loaded, served, or counted.
struct MfcKeyCache {
    MfClassicDeviceKeys keys;
};

static void mfc_key_cache_get_file_path(const uint8_t* uid, size_t uid_len, FuriString* path) {
    furi_string_printf(path, "%s/", MFC_KEY_CACHE_FOLDER);
    for(size_t i = 0; i < uid_len; i++) {
        furi_string_cat_printf(path, "%02X", uid[i]);
    }
    furi_string_cat_str(path, MFC_KEY_CACHE_EXTENSION);
}

static size_t mfc_key_cache_count_keys(const MfcKeyCache* instance) {
    size_t total_keys = 0;
    for(uint8_t i = 0; i < MF_CLASSIC_TOTAL_SECTORS_MAX; i++) {
        total_keys +=
            FURI_BIT(instance->keys.key_a_mask, i) + FURI_BIT(instance->keys.key_b_mask, i);
    }

    return total_keys;
}

static bool mfc_key_cache_read(MfcKeyCache* instance, FlipperFormat* ff, FuriString* temp_str) {
    uint32_t version = 0;
    if(!flipper_format_read_header(ff, temp_str, &version)) return false;
    if(furi_string_cmp_str(temp_str, mfc_key_cache_file_header) != 0) return false;
    if(version != mfc_key_cache_file_version) return false;

    // flipper_format only ever scans forward, so the maps and then the keys have to be read in the
    // order the writer emitted them: both maps, then each sector ascending, A before B. The
    // informational "Mifare Classic type" line the writer puts between the header and the maps has
    // no reader here; the forward scan for "Key A map" simply passes over it.
    if(!flipper_format_read_hex_uint64(ff, "Key A map", &instance->keys.key_a_mask, 1))
        return false;
    if(!flipper_format_read_hex_uint64(ff, "Key B map", &instance->keys.key_b_mask, 1))
        return false;

    for(uint8_t i = 0; i < MF_CLASSIC_TOTAL_SECTORS_MAX; i++) {
        if(FURI_BIT(instance->keys.key_a_mask, i)) {
            furi_string_printf(temp_str, "Key A sector %d", i);
            if(!flipper_format_read_hex(
                   ff,
                   furi_string_get_cstr(temp_str),
                   instance->keys.key_a[i].data,
                   sizeof(MfClassicKey)))
                return false;
        }
        if(FURI_BIT(instance->keys.key_b_mask, i)) {
            furi_string_printf(temp_str, "Key B sector %d", i);
            if(!flipper_format_read_hex(
                   ff,
                   furi_string_get_cstr(temp_str),
                   instance->keys.key_b[i].data,
                   sizeof(MfClassicKey)))
                return false;
        }
    }

    return true;
}

MfcKeyCache* mfc_key_cache_load(Storage* storage, const uint8_t* uid, size_t uid_len) {
    furi_assert(storage);
    furi_assert(uid);

    // No UID names no entry, and an empty one would resolve to the openable path ".../.keys".
    if(uid_len == 0) return NULL;

    FuriString* file_path = furi_string_alloc();
    mfc_key_cache_get_file_path(uid, uid_len, file_path);

    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    MfcKeyCache* instance = malloc(sizeof(MfcKeyCache));

    // Most cards were never saved in the NFC app, so no entry at all is the ordinary outcome.
    const bool file_opened =
        flipper_format_buffered_file_open_existing(ff, furi_string_get_cstr(file_path));
    // A truncated file leaves map bits set for keys that were never read, so anything short of a
    // complete parse is discarded rather than handed back with zeroed keys in the gaps.
    const bool loaded = file_opened && mfc_key_cache_read(instance, ff, temp_str) &&
                        (mfc_key_cache_count_keys(instance) > 0);

    if(!loaded) {
        mfc_key_cache_free(instance);
        instance = NULL;
        // A card with no entry is expected and stays quiet. An entry that exists but was rejected
        // is indistinguishable from it on screen, and the fix -- re-save the card in the NFC app,
        // or a firmware that bumped the format version -- is unguessable without this line.
        if(file_opened) {
            FURI_LOG_W(TAG, "%s unusable, skipping", furi_string_get_cstr(file_path));
        }
    }

    flipper_format_buffered_file_close(ff);
    flipper_format_free(ff);
    furi_string_free(temp_str);
    furi_string_free(file_path);

    return instance;
}

void mfc_key_cache_free(MfcKeyCache* instance) {
    furi_assert(instance);

    free(instance);
}

bool mfc_key_cache_get_key(
    const MfcKeyCache* instance,
    uint8_t sector,
    MfClassicKeyType key_type,
    MfClassicKey* key) {
    furi_assert(instance);
    furi_assert(key);

    if(sector >= MF_CLASSIC_TOTAL_SECTORS_MAX) return false;

    const bool is_key_a = (key_type == MfClassicKeyTypeA);
    const uint64_t mask = is_key_a ? instance->keys.key_a_mask : instance->keys.key_b_mask;
    if(!FURI_BIT(mask, sector)) return false;

    *key = is_key_a ? instance->keys.key_a[sector] : instance->keys.key_b[sector];
    return true;
}
