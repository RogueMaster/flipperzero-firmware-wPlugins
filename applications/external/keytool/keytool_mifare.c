#include "keytool_mifare.h"
#include <stream/stream.h>
#include <stream/file_stream.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Works out whether `block` is a sector-trailer block, for any MIFARE
// Classic variant (Mini / 1K / 4K), and returns the sector number.
// Sectors 0-31 have 4 blocks each (trailer = last block of every 4).
// Sectors 32-39 (4K only) have 16 blocks each, starting at block 128.
static bool keytool_is_trailer_block(uint32_t block, uint32_t* sector_out) {
    if(block < 128) {
        if((block % 4) == 3) {
            *sector_out = block / 4;
            return true;
        }
        return false;
    } else {
        uint32_t idx = block - 128;
        if((idx % 16) == 15) {
            *sector_out = 32 + idx / 16;
            return true;
        }
        return false;
    }
}

bool keytool_mifare_parse_dump(
    Storage* storage,
    const char* path,
    KeyToolKeyEntry* keys_out,
    size_t* key_count_out,
    FuriString* error) {
    *key_count_out = 0;

    Stream* stream = file_stream_alloc(storage);
    if(!file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_set_str(error, "Could not open the selected dump file.");
        stream_free(stream);
        return false;
    }

    FuriString* line = furi_string_alloc();
    size_t key_count = 0;

    while(stream_read_line(stream, line) && key_count < (KEYTOOL_MAX_KEYS - 1)) {
        const char* cline = furi_string_get_cstr(line);
        unsigned int block_num = 0;
        int consumed = 0;

        // Dump lines look like: "Block 3: AA BB CC DD EE FF 78 77 88 00 11 22 33 44 55 66"
        if(sscanf(cline, "Block %u:%n", &block_num, &consumed) != 1) {
            continue;
        }

        uint32_t sector = 0;
        if(!keytool_is_trailer_block(block_num, &sector)) continue;

        // Tokenize the 16 space-separated hex byte pairs that follow.
        const char* p = cline + consumed;
        char tokens[16][3];
        int token_count = 0;
        while(*p && token_count < 16) {
            while(*p == ' ') p++;
            if(!*p) break;
            if(p[0] && p[1]) {
                tokens[token_count][0] = p[0];
                tokens[token_count][1] = p[1];
                tokens[token_count][2] = '\0';
                token_count++;
                p += 2;
            } else {
                break;
            }
        }
        if(token_count < 16) continue; // malformed/short line, ignore it

        // Key A = tokens[0..5], access bits = tokens[6..9], Key B = tokens[10..15].
        // A "??" token means the sector was never authenticated in the
        // original read, so that key is unknown -- not present, not absent.
        bool key_a_valid = true, key_b_valid = true;
        uint8_t key_a[6], key_b[6];

        for(int i = 0; i < 6; i++) {
            if(tokens[i][0] == '?' || tokens[i][1] == '?') {
                key_a_valid = false;
            } else {
                key_a[i] = (uint8_t)strtol(tokens[i], NULL, 16);
            }
        }
        for(int i = 0; i < 6; i++) {
            if(tokens[10 + i][0] == '?' || tokens[10 + i][1] == '?') {
                key_b_valid = false;
            } else {
                key_b[i] = (uint8_t)strtol(tokens[10 + i], NULL, 16);
            }
        }

        if(key_a_valid && key_count < KEYTOOL_MAX_KEYS) {
            memcpy(keys_out[key_count].bytes, key_a, 6);
            keys_out[key_count].sector = sector;
            keys_out[key_count].type = 'A';
            key_count++;
        }
        if(key_b_valid && key_count < KEYTOOL_MAX_KEYS) {
            memcpy(keys_out[key_count].bytes, key_b, 6);
            keys_out[key_count].sector = sector;
            keys_out[key_count].type = 'B';
            key_count++;
        }
    }

    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);

    *key_count_out = key_count;
    if(key_count == 0) {
        furi_string_set_str(
            error, "No readable sector-trailer keys were found in this dump.");
        return false;
    }
    return true;
}
