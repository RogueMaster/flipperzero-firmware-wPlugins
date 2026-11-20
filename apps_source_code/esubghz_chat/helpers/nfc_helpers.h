#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include "../crypto_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The card handed over is an NTAG215, emulated the way real silicon is laid
 * out, because the NFC stack's pollers and the listener both read the fixed
 * pages and the configuration pages and refuse a card that does not look real:
 *
 *   page 0..2    UID and its BCC bytes, plus the internal byte
 *   page 3       capability container
 *   page 4..129  user memory, which is where this app's payload lives
 *   page 130     dynamic lock and the tearing flag
 *   page 131..   configuration pages
 *
 * The payload therefore starts at the first page of user memory and not at
 * page 0 the way the old NFC stack implementation put it. That is a change of
 * the on-air format: a key cannot be shared with an app version older than 1.6.
 * NFC_FORMAT_MAGIC is what makes such a card, or any other NTAG215, fail
 * cleanly instead of being parsed as if it held a key.
 */
#define NFC_USER_PAGE    4
#define NFC_FORMAT_MAGIC 0x45534743UL /* "ESGC" */

/* Most bytes of payload that are written into user memory. */
#define NFC_MAX_BYTES 256

struct FreqNfcEntry {
    uint32_t frequency;
    /* Payload length in bytes, counted from NFC_KEY_OFFSET. The card is
     * emulated at its full size, so the readable pages no longer imply it. */
    uint32_t payload_len;
    uint32_t magic;
    uint32_t unused;
} __attribute__((packed));

struct ReplayDictNfcEntry {
    uint64_t run_id;
    uint32_t counter;
    uint32_t unused;
} __attribute__((packed));

#define NFC_KEY_OFFSET  (NFC_USER_PAGE * MF_ULTRALIGHT_PAGE_SIZE)
#define NFC_FREQ_OFFSET (NFC_KEY_OFFSET + (KEY_BITS / 8))
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
