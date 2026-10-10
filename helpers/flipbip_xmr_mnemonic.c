#include "flipbip_xmr_words.h"
#include <string.h>
// From: lib/crypto
#include <memzero.h>

// Decode word `index` (0..1625) from the front-coded, bit-packed list.
static void flipbip_xmr_word(uint16_t index, char out[FLIPBIP_XMR_WORD_MAX_LEN + 1]) {
    size_t pos = 0;
    size_t len = 0;
    for(uint16_t i = 0; i <= index; i++) {
        uint32_t fields = 0; // 4 bit shared prefix length, 4 bit suffix length
        for(int b = 0; b < 8; b++, pos++) {
            fields = (fields << 1) | ((FLIPBIP_XMR_WORDS_PACKED[pos >> 3] >> (7 - (pos & 7))) & 1);
        }
        len = fields >> 4;
        for(uint32_t n = fields & 0x0f; n > 0; n--) {
            uint8_t letter = 0;
            for(int b = 0; b < 5; b++, pos++) {
                letter = (letter << 1) |
                         ((FLIPBIP_XMR_WORDS_PACKED[pos >> 3] >> (7 - (pos & 7))) & 1);
            }
            out[len++] = 'a' + letter;
        }
    }
    out[len] = '\0';
}

// zlib / IEEE 802.3 CRC-32, bitwise (no 1KB table)
static uint32_t flipbip_crc32(uint32_t crc, const char* data, size_t len) {
    crc = ~crc;
    while(len--) {
        crc ^= (uint8_t)*data++;
        for(int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void flipbip_xmr_mnemonic(const uint8_t spend_key[32], char* out) {
    const uint32_t n = FLIPBIP_XMR_WORDS_NUM;
    uint16_t idx[25];
    char word[FLIPBIP_XMR_WORD_MAX_LEN + 1];

    // Every 4 bytes (little endian) become 3 words
    for(int i = 0; i < 8; i++) {
        const uint32_t x = (uint32_t)spend_key[4 * i] | ((uint32_t)spend_key[4 * i + 1] << 8) |
                           ((uint32_t)spend_key[4 * i + 2] << 16) |
                           ((uint32_t)spend_key[4 * i + 3] << 24);
        const uint32_t w1 = x % n;
        const uint32_t w2 = (x / n + w1) % n;
        const uint32_t w3 = (x / n / n + w2) % n;
        idx[3 * i] = w1;
        idx[3 * i + 1] = w2;
        idx[3 * i + 2] = w3;
    }

    // Checksum word: repeat the word picked by CRC32 of the 3 letter prefixes
    uint32_t crc = 0;
    for(int i = 0; i < 24; i++) {
        flipbip_xmr_word(idx[i], word);
        size_t prefix = strlen(word);
        if(prefix > FLIPBIP_XMR_WORDS_PREFIX_LEN) {
            prefix = FLIPBIP_XMR_WORDS_PREFIX_LEN;
        }
        crc = flipbip_crc32(crc, word, prefix);
    }
    idx[24] = idx[crc % 24];

    char* p = out;
    for(int i = 0; i < 25; i++) {
        flipbip_xmr_word(idx[i], word);
        if(i) *p++ = ' ';
        const size_t len = strlen(word);
        memcpy(p, word, len);
        p += len;
    }
    *p = '\0';

    memzero(idx, sizeof(idx));
    memzero(word, sizeof(word));
}
