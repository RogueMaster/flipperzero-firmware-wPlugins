#include "url_nfc.h"
#include <string.h>

size_t gta_url_ndef_encode(const char* url, uint8_t* out, size_t capacity) {
    if(!url || !out) return 0;
    uint8_t prefix;
    if(strncmp(url, "https://", 8) == 0) {
        prefix = 0x04;
        url += 8;
    } else if(strncmp(url, "http://", 7) == 0) {
        prefix = 0x03;
        url += 7;
    } else {
        return 0;
    }
    const size_t length = strlen(url);
    // A short record and a one-byte TLV length are sufficient for catalog links.
    if(!length || length > 249 || capacity < length + 8) return 0;
    for(size_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)url[i];
        if(c <= 0x20 || c >= 0x7f) return 0;
    }
    out[0] = 0x03; // NDEF TLV
    out[1] = (uint8_t)(length + 5);
    out[2] = 0xD1; // MB, ME, SR, TNF well-known
    out[3] = 1; // type length
    out[4] = (uint8_t)(length + 1);
    out[5] = 'U';
    out[6] = prefix;
    memcpy(out + 7, url, length);
    out[length + 7] = 0xFE;
    return length + 8;
}
