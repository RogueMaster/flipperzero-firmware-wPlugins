/*
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "infinity_figures.h"

#include <string.h>

#include "inf_crypto.h"

const InfFigureInfo inf_figure_table[] = {
#include "infinity_figure_table.inc"
};
const size_t inf_figure_table_count = sizeof(inf_figure_table) / sizeof(inf_figure_table[0]);

/* "(c) Disney 2013", the 31 bytes hashed together with the UID. */
static const uint8_t SHA1_CONSTANT[31] = {0xAF, 0x62, 0xD2, 0xEC, 0x04, 0x91, 0x96, 0x8C,
                                          0xC5, 0x2A, 0x1A, 0x71, 0x65, 0xF8, 0x65, 0xFE,
                                          0x28, 0x63, 0x29, 0x20, 0x44, 0x69, 0x73, 0x6e,
                                          0x65, 0x79, 0x20, 0x32, 0x30, 0x31, 0x33};

const InfFigureInfo* inf_figure_lookup(uint32_t id) {
    for(size_t i = 0; i < inf_figure_table_count; i++) {
        if(inf_figure_table[i].id == id) return &inf_figure_table[i];
    }
    return NULL;
}

InfFigureKind inf_figure_kind(uint32_t id) {
    const uint32_t range = id >> 16;
    if(range == 0x0F) return InfKindCharacter; /* 0x0F4241.. */
    if(range == 0x1E) return InfKindPlaySet; /* 0x1E8481.. */
    return InfKindDisc; /* 0x2DC6C1.. and 0x3D0900.. */
}

/* AES key for a figure: SHA-1(constant || uid7), each 32-bit word byte-swapped. */
static void derive_key(const uint8_t uid7[7], uint8_t key[16]) {
    InfSha1 sha;
    uint8_t digest[20];
    inf_sha1_init(&sha);
    inf_sha1_update(&sha, SHA1_CONSTANT, sizeof(SHA1_CONSTANT));
    inf_sha1_update(&sha, uid7, 7);
    inf_sha1_final(&sha, digest);
    for(int i = 0; i < 4; i++) {
        for(int x = 0; x < 4; x++) {
            key[x + (i * 4)] = digest[(3 - x) + (i * 4)];
        }
    }
}

/* CRC-32 (reflected, polynomial 0xEDB88320) with no initial or final inversion. */
static uint32_t figure_crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0;
    for(size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for(int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0u);
        }
    }
    return crc;
}

uint32_t inf_figure_decode_number(const uint8_t data[INF_FIGURE_SIZE]) {
    uint8_t key[16];
    uint8_t plain[16];
    InfAes128 aes;
    derive_key(data, key);
    inf_aes128_set_key(&aes, key);
    inf_aes128_decrypt_block(&aes, data + INF_BLOCK_SIZE, plain);
    return ((uint32_t)plain[1] << 16) | ((uint32_t)plain[2] << 8) | (uint32_t)plain[3];
}

bool inf_figure_create_blank(
    uint8_t out[INF_FIGURE_SIZE],
    uint32_t figure_number,
    uint8_t series,
    const uint8_t uid7[7]) {
    memset(out, 0, INF_FIGURE_SIZE);

    /* Standard read/write access bits in each sector trailer. */
    static const uint8_t first_trailer[3] = {0x17, 0x87, 0x8E};
    static const uint8_t other_trailer[3] = {0x77, 0x87, 0x88};
    memcpy(&out[0x36], first_trailer, 3);
    for(int sector = 1; sector < 5; sector++) {
        memcpy(&out[(sector * 0x40) + 0x36], other_trailer, 3);
    }

    uint8_t uid_block[16] = {0};
    memcpy(uid_block, uid7, 7);
    uid_block[7] = 0x89;
    uid_block[8] = 0x44;
    uid_block[9] = 0x00;
    uid_block[10] = 0xC2;

    uint8_t figure_block[16] = {0};
    figure_block[1] = (uint8_t)((figure_number >> 16) & 0xFF);
    figure_block[2] = (uint8_t)((figure_number >> 8) & 0xFF);
    figure_block[3] = (uint8_t)(figure_number & 0xFF);
    figure_block[9] = series;
    figure_block[10] = 0xD1;
    figure_block[11] = 0x1F;

    /* Manufacture date (YY/MM/DD) is the release date of the figure's series. */
    if(series == 1) {
        figure_block[4] = 0x0D;
        figure_block[5] = 0x08;
        figure_block[6] = 0x12;
    } else if(series == 2) {
        figure_block[4] = 0x0E;
        figure_block[5] = 0x09;
        figure_block[6] = 0x12;
    } else if(series == 3) {
        figure_block[4] = 0x0F;
        figure_block[5] = 0x08;
        figure_block[6] = 0x1C;
    }

    const uint32_t crc = figure_crc32(figure_block, 12);
    for(int i = 0; i < 4; i++) {
        figure_block[12 + i] = (uint8_t)((crc >> ((3 - i) * 8)) & 0xFF);
    }

    if(figure_block[1] == 0) return false;

    uint8_t key[16];
    InfAes128 aes;
    derive_key(uid7, key);
    inf_aes128_set_key(&aes, key);

    uint8_t encrypted_block[16];
    uint8_t encrypted_blank[16];
    static const uint8_t blank_block[16] = {0};
    inf_aes128_encrypt_block(&aes, figure_block, encrypted_block);
    inf_aes128_encrypt_block(&aes, blank_block, encrypted_blank);

    memcpy(&out[0], uid_block, 16);
    memcpy(&out[16], encrypted_block, 16);
    memcpy(&out[16 * 0x04], encrypted_blank, 16);
    memcpy(&out[16 * 0x08], encrypted_blank, 16);
    memcpy(&out[16 * 0x0C], encrypted_blank, 16);
    memcpy(&out[16 * 0x0D], encrypted_blank, 16);
    return true;
}
