/*
 * TagTinker — ESL NFC tag decoder (implementation)
 *
 * Supported ESL tags carry an NDEF URI whose last path segment is a
 * 10-character id in a custom base64 alphabet. This module walks the tag's TLV
 * area, pulls out the URI, and decodes that id into the 17-character barcode
 * format expected by tagtinker_barcode_to_plid().
 */

#include "tagtinker_nfc.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* Direct ASCII-to-index lookup table, -1 = not in alphabet */
static const int8_t CHAR_LUT[128] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,35,-1,-1,
    28,24,30,38,58, 5,23, 6, 3,40,-1,-1,-1,-1,-1,-1,
    -1,20,15,54,16,44,46,63, 4,48,34,19,37, 0,26, 1,
     8,41,31, 2,45,55,60,12,11,57,33,-1,-1,-1,-1,50,
    -1,13,39, 9,43,18,29,52,59, 7,61,62,14,25,32,56,
    42,47,53,22,36,49,10,21,17,27,51,-1,-1,-1,-1,-1,
};

static int alphabet_index(char c) {
    uint8_t idx = (uint8_t)c;
    if(idx >= 128) return -1;
    return CHAR_LUT[idx];
}

static uint32_t decode_b64(const char* s, int len) {
    uint32_t r = 0;
    for(int i = 0; i < len; i++) {
        int idx = alphabet_index(s[(len - 1) - i]);
        if(idx < 0) return 0;
        r = (r * 64) + (uint32_t)idx;
    }
    return r;
}

static bool decode_tag(const char* tag, char barcode[18]) {
    if(strlen(tag) != 10) return false;

    for(int i = 0; i < 10; i++) {
        if(alphabet_index(tag[i]) < 0) return false;
    }

    uint32_t val1 = decode_b64(tag + 5, 5);
    uint32_t val2 = decode_b64(tag, 5);

    /* Each value is 30 bits, so it can reach 10 digits; give snprintf room for
     * both fields plus the terminator instead of letting it truncate. */
    char raw[24];
    snprintf(raw, sizeof(raw), "%09lu%09lu", (unsigned long)val1, (unsigned long)val2);
    if(strlen(raw) != 18) return false;

    int lc = (raw[0] - '0') * 10 + (raw[1] - '0');
    if(lc > 25) return false;
    char letter = (char)(lc + 65);

    barcode[0] = letter;
    memcpy(barcode + 1, raw + 2, 16);
    barcode[17] = '\0';

    if(barcode[1] != '4') return false;

    int cs = 0;
    for(int i = 0; i < 16; i++) {
        char c = barcode[i];
        cs += (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    return (cs % 10) == (barcode[16] - '0');
}

/* Read one byte of the TLV area, which starts at page 4. Returns false once
 * the offset runs past the pages the poller actually managed to read. */
static bool tlv_byte(const MfUltralightData* mfu, uint32_t offset, uint8_t* out) {
    uint32_t page = 4U + (offset / 4U);
    if(page >= mfu->pages_read) return false;
    *out = mfu->page[page].data[offset % 4U];
    return true;
}

/* Locate the NDEF TLV (type 0x03), skipping NULL / lock-control /
 * memory-control TLVs that factory-formatted NTAG21x tags put in front of it. */
static bool
    find_ndef_tlv(const MfUltralightData* mfu, uint32_t* value_start, uint32_t* value_len) {
    uint32_t offset = 0;
    uint8_t type = 0;

    while(tlv_byte(mfu, offset, &type)) {
        offset++;
        if(type == 0x00) continue; /* NULL TLV: no length and no value */
        if(type == 0xFE) return false; /* terminator */

        uint8_t len_byte = 0;
        if(!tlv_byte(mfu, offset, &len_byte)) return false;
        offset++;

        uint32_t len = len_byte;
        if(len_byte == 0xFF) { /* three-byte length form */
            uint8_t hi = 0, lo = 0;
            if(!tlv_byte(mfu, offset, &hi) || !tlv_byte(mfu, offset + 1, &lo)) return false;
            offset += 2;
            len = ((uint32_t)hi << 8) | lo;
        }

        if(type == 0x03) {
            *value_start = offset;
            *value_len = len;
            return true;
        }
        offset += len;
    }
    return false;
}

/* Extract the URI body and report the NDEF URI prefix code alongside it. */
static bool
    extract_uri(const MfUltralightData* mfu, char* url, size_t url_size, uint8_t* prefix_code) {
    if(!mfu || !url || url_size == 0) return false;
    url[0] = '\0';

    if(mfu->pages_read < 5) return false;
    if(mfu->page[3].data[0] != 0xE1) return false; /* NDEF capability container */

    uint32_t start = 0, value_len = 0;
    if(!find_ndef_tlv(mfu, &start, &value_len)) return false;
    if(value_len < 4) return false;

    uint8_t header = 0, type_len = 0, payload_len = 0;
    uint32_t offset = start;
    if(!tlv_byte(mfu, offset++, &header)) return false;
    if(!(header & 0x10)) return false; /* short record only */
    if((header & 0x07) != 0x01) return false; /* TNF must be NFC Forum well-known */
    if(!tlv_byte(mfu, offset++, &type_len)) return false;
    if(!tlv_byte(mfu, offset++, &payload_len)) return false;

    uint8_t id_len = 0;
    if(header & 0x08) { /* IL flag: an ID length byte follows */
        if(!tlv_byte(mfu, offset++, &id_len)) return false;
    }

    if(type_len != 1) return false;
    uint8_t type = 0;
    if(!tlv_byte(mfu, offset, &type)) return false;
    if(type != 0x55) return false; /* 'U' = URI record */
    offset += type_len;
    offset += id_len;

    if(payload_len < 2) return false;

    /* payload[0] is the URI prefix code ("https://" and friends); the body
     * follows it. Copy the body only, bounded by the record length, the
     * caller's buffer and the pages we actually read. */
    uint8_t prefix = 0;
    if(!tlv_byte(mfu, offset, &prefix)) return false;
    if(prefix_code) *prefix_code = prefix;

    size_t out = 0;
    for(uint32_t i = 1; i < payload_len; i++) {
        uint8_t c = 0;
        if(!tlv_byte(mfu, offset + i, &c)) break;
        if(c == 0x00 || c == 0xFE) break;
        if(out + 1 >= url_size) break;
        url[out++] = (char)c;
    }
    url[out] = '\0';
    return out > 0;
}

bool tagtinker_nfc_extract_url(const MfUltralightData* mfu, char* url, size_t url_size) {
    return extract_uri(mfu, url, url_size, NULL);
}

bool tagtinker_nfc_decode_url(const char* url, char barcode[18]) {
    if(!url || !barcode) return false;
    barcode[0] = '\0';

    const char* last_slash = strrchr(url, '/');
    if(!last_slash) return false;
    return decode_tag(last_slash + 1, barcode);
}

bool tagtinker_nfc_url_host_is(const char* url, const char* host) {
    if(!url || !host || !*host) return false;

    /* NDEF URI bodies normally have the scheme stripped into the prefix code,
     * but a record written with prefix code 0x00 keeps it inline. A "://"
     * after the first '/', '?' or '#' belongs to the path, query or fragment. */
    const char* scheme_end = strstr(url, "://");
    if(scheme_end && scheme_end < url + strcspn(url, "/?#")) url = scheme_end + 3;

    size_t host_len = strlen(host);
    if(strncasecmp(url, host, host_len) != 0) return false;

    char next = url[host_len];
    return next == '\0' || next == '/' || next == ':' || next == '?' || next == '#';
}

bool tagtinker_nfc_decode_barcode(const MfUltralightData* mfu_data, char barcode[18]) {
    if(!mfu_data || !barcode) return false;
    barcode[0] = '\0';

    char url[TAGTINKER_NFC_URL_LEN];
    if(!tagtinker_nfc_extract_url(mfu_data, url, sizeof(url))) return false;
    return tagtinker_nfc_decode_url(url, barcode);
}

TagTinkerNfcResult tagtinker_nfc_classify(const MfUltralightData* mfu_data, char barcode[18]) {
    if(barcode) barcode[0] = '\0';

    /* The firmware poller reports success with zero pages when a chip was
     * activated but its first page read failed. Any NFC-A chip that is not an
     * Ultralight/NTAG ends up here, as can a tag that moved during the read. */
    if(!mfu_data || mfu_data->pages_read == 0) return TagTinkerNfcResultUnreadable;

    char url[TAGTINKER_NFC_URL_LEN];
    uint8_t prefix = 0;
    if(!extract_uri(mfu_data, url, sizeof(url), &prefix)) {
        return TagTinkerNfcResultUnrecognized;
    }

    if(barcode && tagtinker_nfc_decode_url(url, barcode)) return TagTinkerNfcResultDecoded;
    if(barcode) barcode[0] = '\0';

    /* The one public VUSION label dump (i12bp8/TagTinker#51) carries
     * https://nfc.imagotag.com/<id>. Match that host exactly rather than any
     * URL that happens to contain the word. Prefix codes 0x01/0x02 add "www."
     * to the host, and other codes are not web links, so only accept
     * http:// / https:// as prefix code 0x03 / 0x04 or written inline after
     * prefix code 0x00. */
    bool inline_web = prefix == 0x00 && (strncasecmp(url, "http://", 7) == 0 ||
                                         strncasecmp(url, "https://", 8) == 0);
    bool web_link = prefix == 0x03 || prefix == 0x04 || inline_web;
    if(web_link && tagtinker_nfc_url_host_is(url, "nfc.imagotag.com")) {
        return TagTinkerNfcResultImagotagLink;
    }

    return TagTinkerNfcResultUnrecognized;
}
