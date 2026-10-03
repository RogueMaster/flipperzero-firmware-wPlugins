#pragma once

#include <nfc/protocols/mf_classic/mf_classic.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read-only twin of the NFC app's /ext/nfc/.cache/<UID>.keys reader. Saving a card in the NFC app
// writes every key it recovered there under the card's UID -- including the keys of a static
// encrypted nonce tag, which MFKey32 only ever puts in the per-UID (CUID) dictionary. A magic
// clone carries that same UID, so the entry is the clone's keys too. The firmware's own reader is
// internal to the NFC app and not exported through the API, hence this local copy; it never
// writes, so it cannot corrupt an entry the NFC app owns.
typedef struct MfcKeyCache MfcKeyCache;

/** Load the entry named by uid. Returns NULL, having allocated nothing the caller must release,
 * when uid_len is 0, there is no entry, the header or version doesn't match, a key one of the maps
 * promises isn't in the file, or the entry holds no keys at all. A half-parsed entry is never
 * returned, so a non-NULL result always has at least one key that came out of the file. */
MfcKeyCache* mfc_key_cache_load(Storage* storage, const uint8_t* uid, size_t uid_len);

void mfc_key_cache_free(MfcKeyCache* instance);

/** Read one key. Returns false when the entry holds no key of that type for that sector. */
bool mfc_key_cache_get_key(
    const MfcKeyCache* instance,
    uint8_t sector,
    MfClassicKeyType key_type,
    MfClassicKey* key);

#ifdef __cplusplus
}
#endif
