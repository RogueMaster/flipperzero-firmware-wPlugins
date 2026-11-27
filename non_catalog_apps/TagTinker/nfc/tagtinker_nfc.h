/*
 * TagTinker — ESL NFC tag decoder
 *
 * Reads the NDEF URI from a Mifare Ultralight / NTAG tag and decodes the
 * ESL id in its last path segment into the 17-character barcode format used
 * by TagTinker.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

/* Longest NDEF URI body (scheme prefix excluded) we keep. */
#define TAGTINKER_NFC_URL_LEN 96

/* Extract the NDEF URI body (without the "https://" style prefix) from a tag.
 * Walks the TLV area so lock/memory-control TLVs before the NDEF TLV are
 * skipped, and reads as many pages as the record actually spans. */
bool tagtinker_nfc_extract_url(const MfUltralightData* mfu_data, char* url, size_t url_size);

/* Decode the ESL id in the URL's last path segment into a barcode. */
bool tagtinker_nfc_decode_url(const char* url, char barcode[18]);

/* True when the URL's host is `host` (case-insensitive). The URL may be an
 * NDEF URI body without a scheme, or a full URL with one. The caller decides
 * whether the stripped NDEF prefix code (for example "https://www.") changes
 * the real host. */
bool tagtinker_nfc_url_host_is(const char* url, const char* host);

/* Convenience: extract the URL from the tag and decode it in one call. */
bool tagtinker_nfc_decode_barcode(const MfUltralightData* mfu_data, char barcode[18]);

/* What a completed read contains, from the app's point of view. None of these
 * results identifies a vendor or a display technology; they only describe the
 * NFC data. */
typedef enum {
    /* A chip was activated but no page could be read: not an Ultralight/NTAG,
     * or a tag that moved during the read. */
    TagTinkerNfcResultUnreadable,
    /* The NDEF URI carries an id TagTinker decodes; barcode is filled. */
    TagTinkerNfcResultDecoded,
    /* No decodable id, and the NDEF URI is an http:// or https:// link whose
     * host is nfc.imagotag.com. */
    TagTinkerNfcResultImagotagLink,
    /* Readable, but no NDEF URI or no decodable id in it. */
    TagTinkerNfcResultUnrecognized,
} TagTinkerNfcResult;

/* Classify a completed read. barcode is filled only for ..Decoded and is an
 * empty string otherwise. */
TagTinkerNfcResult tagtinker_nfc_classify(const MfUltralightData* mfu_data, char barcode[18]);
