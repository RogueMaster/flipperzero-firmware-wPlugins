// SPDX-License-Identifier: GPL-3.0
#include "probe_disk.h"
#include "pc_settings.h"
#include <stdio.h>
#include <string.h>
static void le16(uint8_t* p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}
static void le32(uint8_t* p, uint32_t v) {
    for(unsigned i = 0; i < 4; i++)
        p[i] = v >> (8 * i);
}
bool pc_token_valid(const char* s) {
    if(!s || strlen(s) != 18 || s[0] != 'P' || s[1] != 'C') return false;
    for(unsigned i = 2; i < 18; i++)
        if(!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'A' && s[i] <= 'F'))) return false;
    return true;
}

static const char readme[] =
    "USB PurpleCheck: synthetic test files only.\r\nCopy SEND.TXT to the PC, "
    "then copy it back here as RETURN.TXT.\r\nSafely eject the volume before "
    "stopping the test. No private files are requested.\r\n";
static void fat12_set(uint8_t* fat, unsigned cluster, unsigned value) {
    unsigned o = cluster + cluster / 2;
    if(cluster & 1) {
        fat[o] = (fat[o] & 15) | ((value & 15) << 4);
        fat[o + 1] = value >> 4;
    } else {
        fat[o] = value;
        fat[o + 1] = (fat[o + 1] & 240) | ((value >> 8) & 15);
    }
}
static void entry(uint8_t* p, const char* name, uint16_t cluster, uint32_t size) {
    memcpy(p, name, 11);
    p[11] = 0x20;
    le16(p + 26, cluster);
    le32(p + 28, size);
    le16(p + 24, (uint16_t)((46 << 9) | (11 << 5) | 10));
}
void pc_payload_sector(const char* token, uint32_t index, uint8_t out[PC_BLOCK_SIZE]) {
    memset(out, ' ', PC_BLOCK_SIZE);
    for(unsigned row = 0; row < 8; row++) {
        char line[96];
        int n = snprintf(
            line,
            sizeof(line),
            "PURPLECHECK SYNTHETIC %s B%02lu L%02u",
            token,
            (unsigned long)index,
            row);
        if(n > 0) memcpy(out + row * 64, line, (size_t)n < 62 ? (size_t)n : 62);
        out[row * 64 + 62] = '\r';
        out[row * 64 + 63] = '\n';
    }
}
void pc_disk_sector_config(uint32_t lba, const char* token, bool files, uint8_t out[PC_BLOCK_SIZE]) {
    memset(out, 0, PC_BLOCK_SIZE);
    if(lba == 0) {
        out[0] = 0xEB;
        out[1] = 0x3C;
        out[2] = 0x90;
        memcpy(out + 3, "RF5PC   ", 8);
        le16(out + 11, 512);
        out[13] = 1;
        le16(out + 14, 1);
        out[16] = 2;
        le16(out + 17, 32);
        le16(out + 19, PC_BLOCKS);
        out[21] = 0xF8;
        le16(out + 22, 6);
        le16(out + 24, 32);
        le16(out + 26, 2);
        out[38] = 0x29;
        le32(out + 39, 0x52463550);
        memcpy(out + 43, "PURPLECHECK", 11);
        memcpy(out + 54, "FAT12   ", 8);
        out[510] = 0x55;
        out[511] = 0xAA;
    } else if(lba == 1 || lba == 7) {
        out[0] = 0xF8;
        out[1] = 0xFF;
        out[2] = 0xFF;
        fat12_set(out, 2, 0xFFF);
        if(files) {
            for(unsigned c = 3; c <= 19; c++)
                fat12_set(out, c, (c == 10 || c == 18 || c == 19) ? 0xFFF : c + 1);
        }
    } else if(lba == PC_ROOT_START) {
        memcpy(out, "PURPLECHECK", 11);
        out[11] = 8;
        char text[256];
        int n = snprintf(
            text,
            sizeof(text),
            "USB PurpleCheck 0.3\r\nTOKEN=%s\r\nSynthetic test "
            "volume. No personal SD files exposed.\r\n",
            token);
        entry(out + 32, "PROBE   TXT", 2, n);
        if(files) {
            entry(out + 64, "SEND    TXT", 3, PC_PAYLOAD_BYTES);
            entry(out + 96, "RETURN  TXT", 11, PC_PAYLOAD_BYTES);
            entry(out + 128, "README  TXT", 19, sizeof(readme) - 1);
        }
    } else if(lba == PC_DATA_START) {
        snprintf(
            (char*)out,
            PC_BLOCK_SIZE,
            "USB PurpleCheck 0.3\r\nTOKEN=%s\r\nSynthetic test volume. No "
            "personal SD files exposed.\r\n",
            token);
    } else if(files && lba >= PC_SEND_START && lba < PC_SEND_START + PC_SEND_BLOCKS)
        pc_payload_sector(token, lba - PC_SEND_START, out);
    else if(files && lba == 32)
        memcpy(out, readme, sizeof(readme) - 1);
}
void pc_disk_sector(uint32_t lba, const char* token, uint8_t out[PC_BLOCK_SIZE]) {
    pc_disk_sector_config(lba, token, true, out);
}
static uint16_t read16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t read32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static bool next_cluster(PcSectorRead read, void* ctx, uint16_t cluster, uint16_t* next) {
    uint32_t offset = cluster + cluster / 2, lba = 1 + offset / 512;
    uint8_t b[512];
    if(lba > 6 || !read(ctx, lba, b)) return false;
    uint8_t lo = b[offset % 512], hi;
    if(offset % 512 == 511) {
        if(lba >= 6 || !read(ctx, lba + 1, b)) return false;
        hi = b[0];
    } else
        hi = b[offset % 512 + 1];
    uint16_t value = (uint16_t)lo | ((uint16_t)hi << 8);
    *next = (cluster & 1) ? value >> 4 : value & 0xFFF;
    return true;
}
PcReturnResult pc_verify_return(PcSectorRead read, void* ctx, const char* token, uint32_t* crc) {
    uint8_t b[512], expected[512];
    uint16_t cluster = 0;
    uint32_t size = 0;
    bool found = false;
    *crc = 0;
    for(unsigned r = PC_ROOT_START; r < PC_DATA_START; r++) {
        if(!read(ctx, r, b)) return PcReturnIoError;
        for(unsigned i = 0; i < 512; i += 32)
            if(!memcmp(b + i, "RETURN  TXT", 11) && !(b[i + 11] & 0x18)) {
                cluster = read16(b + i + 26);
                size = read32(b + i + 28);
                found = true;
                break;
            }
        if(found) break;
    }
    if(!found || size == 0) return PcReturnNone;
    if(size != PC_PAYLOAD_BYTES) return PcReturnChanged;
    bool exact = true, nonzero = false;
    for(unsigned i = 0; i < PC_SEND_BLOCKS; i++) {
        if(cluster < 2 || cluster >= PC_BLOCKS - PC_DATA_START + 2) return PcReturnIoError;
        if(!read(ctx, PC_DATA_START + cluster - 2, b)) return PcReturnIoError;
        pc_payload_sector(token, i, expected);
        if(memcmp(b, expected, 512)) exact = false;
        for(unsigned j = 0; j < 512; j++)
            if(b[j]) nonzero = true;
        *crc = pc_crc32(*crc, b, 512);
        uint16_t next;
        if(!next_cluster(read, ctx, cluster, &next)) return PcReturnIoError;
        if(i + 1 == PC_SEND_BLOCKS) {
            if(next < 0xFF8) return PcReturnIoError;
        } else if(next < 2 || next >= 0xFF0)
            return PcReturnIoError;
        cluster = next;
    }
    return exact ? PcReturnMatch : nonzero ? PcReturnChanged : PcReturnNone;
}
