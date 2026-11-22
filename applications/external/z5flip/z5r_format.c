#include "z5r.h"
#include <string.h>
#include <stdio.h>

static const uint8_t attr_bits[5] = {0, 0x40, 0x60, 0x20, 0x00}; /* index = Z5rAttr */

uint16_t z5r_mem_size_from_code(uint8_t code) {
    if(code == 0x05) return 4096;
    if(code == 0x15) return 8192;
    return 0; /* 0x3A/0x7A = 16/32 KB "Z7" layout (Z-5R mod.5000, Matrix): not supported */
}

uint16_t z5r_capacity(uint16_t mem_size) {
    return mem_size == 4096 ? 680 : Z5R_KEYS_MAX;
}

static uint8_t attr_from_bits(uint8_t b5) {
    switch((b5 >> 5) & 3) {
    case 2:
        return Z5rAttrNormal;
    case 3:
        return Z5rAttrBlocking;
    case 1:
        return Z5rAttrMaster;
    default:
        return Z5rAttrInactive;
    }
}

bool z5r_parse(Z5rDb* db) {
    const uint8_t* m = db->mem;
    db->count = 0;
    uint16_t size = z5r_mem_size_from_code(m[5]);
    if(!size) return false;
    db->mem_size = size;
    uint16_t cap = z5r_capacity(size);

    if(m[0] < 0x11) {
        /* legacy layout: 4-byte records from offset 8: [flags][3-byte key], flags bit2 = end, ==1 master */
        for(size_t off = 8; off + 4 <= size && db->count < cap; off += 4) {
            if(m[off] & 4) break;
            Z5rKey* k = &db->keys[db->count++];
            memset(k, 0, sizeof(*k));
            memcpy(k->b, &m[off + 1], 3);
            k->len = 3;
            k->attr = m[off] == 1 ? Z5rAttrMaster : Z5rAttrNormal;
        }
        return true;
    }

    for(size_t off = Z5R_HDR; off + 6 <= size && db->count < cap; off += 6) {
        uint8_t b5 = m[off + 5];
        if(b5 & 0x80) break;
        Z5rKey* k = &db->keys[db->count++];
        memset(k, 0, sizeof(*k));
        if(b5 & 0x10) {
            memcpy(k->b, &m[off], 3);
            k->len = 3;
        } else {
            memcpy(k->b, &m[off], 5);
            k->b[5] = b5 & 0x0F;
            k->len = 6;
        }
        k->attr = attr_from_bits(b5);
    }
    return true;
}

void z5r_init_blank(Z5rDb* db, uint16_t mem_size) {
    memset(db->mem, 0xFF, sizeof(db->mem));
    db->mem[0] = 0x33;
    db->mem[1] = 0x00;
    db->mem[3] = 0xFF; /* factory open-time code 0x002E, same as BaseZ5R "clear" */
    db->mem[4] = 0xD1;
    db->mem[5] = mem_size == 4096 ? 0x05 : 0x15;
    db->mem[Z5R_HDR + 5] = 0xE0;
    db->mem_size = mem_size;
    db->count = 0;
}

size_t z5r_build(Z5rDb* db) {
    uint8_t* m = db->mem;
    uint16_t cap = z5r_capacity(db->mem_size);
    uint16_t n = db->count > cap ? cap : db->count; /* callers check the fit first; never drop keys from the base here */
    memset(m + Z5R_HDR, 0xFF, sizeof(db->mem) - Z5R_HDR);
    m[0] = 0x34; /* "in progress"; caller writes page 0 again with 0x33 last, like BaseZ5R */
    m[5] = db->mem_size == 4096 ? 0x05 : 0x15;
    uint8_t* r = m + Z5R_HDR;
    for(uint16_t i = 0; i < n; i++, r += 6) {
        const Z5rKey* k = &db->keys[i];
        uint8_t a = attr_bits[k->attr <= 4 ? k->attr : 1];
        if(k->len == 3) {
            memcpy(r, k->b, 3);
            r[5] = 0x10 | a;
        } else {
            memcpy(r, k->b, 5);
            r[5] = (k->b[5] & 0x0F) | a;
        }
    }
    r[5] = 0xE0; /* terminator; cap always leaves one free slot */
    return (size_t)(r + 6 - m);
}

void z5r_key_from_rfid(bool em, const uint8_t* d, Z5rKey* k) {
    uint8_t n = em ? 5 : 3;
    memset(k->b, 0, sizeof(k->b));
    for(uint8_t i = 0; i < n; i++) k->b[i] = d[n - 1 - i];
    k->len = em ? 6 : 3; /* EM4100 is a 6-byte record with b5 = 0, as a real controller stores it */
}

uint16_t z5r_open_time_code(const Z5rDb* db) {
    return (uint16_t)~((db->mem[3] << 8) | db->mem[4]);
}

int z5r_find(const Z5rDb* db, const Z5rKey* k) {
    for(uint16_t i = 0; i < db->count; i++) {
        if(db->keys[i].len == k->len && !memcmp(db->keys[i].b, k->b, k->len)) return i;
    }
    return -1;
}

const char* z5r_attr_name(uint8_t attr) {
    static const char* n[] = {"?", "Normal", "Blocking", "Master", "Inactive"};
    return n[attr <= 4 ? attr : 0];
}

void z5r_key_str(const Z5rKey* k, char* out, size_t n) {
    if(k->len == 3)
        snprintf(out, n, "%02X%02X%02X", k->b[2], k->b[1], k->b[0]);
    else
        snprintf(
            out, n, "%02X%02X%02X%02X%02X%02X", k->b[5], k->b[4], k->b[3], k->b[2], k->b[1], k->b[0]);
}
