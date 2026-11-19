#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include "../crypto_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NFC_MAX_BYTES    256
#define NFC_CONFIG_PAGES 4

struct FreqNfcEntry {
    uint32_t frequency;
    uint32_t unused1;
    uint32_t unused2;
    uint32_t unused3;
} __attribute__((packed));

struct ReplayDictNfcEntry {
    uint64_t run_id;
    uint32_t counter;
    uint32_t unused;
} __attribute__((packed));

/* Layout of the emulated NTAG215 memory used to hand a key over to another
 * Flipper. This is the on-air format of the old NFC stack implementation and
 * must not change, otherwise key sharing breaks between app versions.
 *
 *   page 0..7   key (KEY_BITS / 8 bytes)
 *   page 8..11  struct FreqNfcEntry
 *   page 12..   struct ReplayDictNfcEntry[]
 *   last 4 pages (NFC_CONFIG_PAGES) are the trailing config pages
 */
#define NFC_KEY_OFFSET  (0)
#define NFC_FREQ_OFFSET (KEY_BITS / 8)
#define NFC_DICT_OFFSET (NFC_FREQ_OFFSET + sizeof(struct FreqNfcEntry))

/* The pages of an MfUltralightData are a plain array of 4 byte chunks, so the
 * card memory can be addressed as one flat buffer. */
static inline uint8_t* nfc_dev_data_bytes(MfUltralightData* data) {
    return (uint8_t*)data->page;
}

static inline size_t nfc_dev_data_bytes_read(const MfUltralightData* data) {
    return (size_t)data->pages_read * MF_ULTRALIGHT_PAGE_SIZE;
}

/* mf_ultralight_alloc() does not zero the data it returns and the app puts key
 * material in there, so wipe everything but the owned ISO14443-3A data. */
static inline void nfc_dev_data_clear(MfUltralightData* data) {
    Iso14443_3aData* iso14443_3a_data = data->iso14443_3a_data;

    crypto_explicit_bzero(data, sizeof(MfUltralightData));
    crypto_explicit_bzero(iso14443_3a_data, sizeof(Iso14443_3aData));

    data->iso14443_3a_data = iso14443_3a_data;
}

#ifdef __cplusplus
}
#endif
