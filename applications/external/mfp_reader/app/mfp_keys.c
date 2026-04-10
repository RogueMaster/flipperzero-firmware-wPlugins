#include "mfp_keys.h"

#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

/* ---- Hardcoded default keys ---- */

const MfpKey mfp_default_keys[MFP_DEFAULT_KEY_COUNT] = {
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF},
    {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF},
    {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7,0xD3,0xF7,0xD3,0xF7,0xD3,0xF7,0xD3,0xF7,0xD3,0xF7},
    {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10,0x11,0x12,0x13,0x14,0x15,0x16},
};

/* ---- Sector geometry helpers ---- */

uint8_t mfp_sector_first_block(MfpCardSize size, uint8_t sector) {
    if(size == MfpSize4K && sector >= 32) {
        return 128 + (uint8_t)((sector - 32) * 16);
    }
    return sector * 4;
}

uint8_t mfp_sector_block_count(MfpCardSize size, uint8_t sector) {
    if(size == MfpSize4K && sector >= 32) {
        return 16;
    }
    return 4;
}

uint8_t mfp_sector_count(MfpCardSize size) {
    return (size == MfpSize4K) ? MFP_SECTORS_4K : MFP_SECTORS_2K;
}

/* ---- Dictionary loader ---- */

uint32_t mfp_keys_load_dict(Storage* storage, const char* path, uint8_t** out_buf) {
    *out_buf = NULL;

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return 0;
    }

    uint64_t fsize = storage_file_size(f);
    if(fsize == 0) {
        storage_file_close(f);
        storage_file_free(f);
        return 0;
    }
    if(fsize > 64 * 1024) fsize = 64 * 1024;

    char* raw = malloc((size_t)fsize + 1);
    if(!raw) {
        storage_file_close(f);
        storage_file_free(f);
        return 0;
    }

    size_t got = storage_file_read(f, raw, (size_t)fsize);
    raw[got] = '\0';
    storage_file_close(f);
    storage_file_free(f);

    /* Upper bound on key count */
    uint32_t max_keys = (uint32_t)(got / 32) + 1;
    uint8_t* buf = malloc(max_keys * MFP_AES_KEY_SIZE);
    if(!buf) {
        free(raw);
        return 0;
    }

    uint32_t count = 0;
    char* p = raw;
    while(*p && count < max_keys) {
        while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if(!*p) break;

        /* Skip comment lines */
        if(*p == '#') {
            while(*p && *p != '\n') p++;
            continue;
        }

        /* Collect hex digits from this line */
        char hex[33] = {0};
        int hlen = 0;
        while(*p && *p != '\n' && *p != '\r') {
            char c = *p++;
            if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                if(hlen < 32) hex[hlen++] = c;
            }
        }

        if(hlen == 32) {
            uint8_t* key = buf + count * MFP_AES_KEY_SIZE;
            for(int i = 0; i < MFP_AES_KEY_SIZE; i++) {
                char byte_str[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
                key[i] = (uint8_t)strtoul(byte_str, NULL, 16);
            }
            count++;
        }
        while(*p && *p != '\n') p++;
    }

    free(raw);

    if(count == 0) {
        free(buf);
        return 0;
    }

    *out_buf = buf;
    return count;
}
