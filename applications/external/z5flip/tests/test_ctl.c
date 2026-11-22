/* Host tests for the key format and controller operations with a fake EEPROM (ufbt also compiles this folder, hence the guard).
   From this folder (Linux / WSL):
   gcc -Wall -Wextra -DZ5R_HOST_TEST -I.. test_ctl.c ../z5r_ctl.c ../z5r_format.c -o t && ./t */
#ifdef Z5R_HOST_TEST
#include "../z5r.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t eeprom[8192];
static uint16_t chip = 8192; /* 4096 = 24C32: address bit 12 ignored, 0x1000 aliases 0x0000 */
static int last_page_written = -1, writes, fail_read_at = -1;
bool z5r_link_present(void) {
    return true;
}
bool z5r_link_read(uint16_t a, uint8_t* b, size_t n) {
    if(a == fail_read_at) return false;
    for(size_t i = 0; i < n; i++)
        b[i] = eeprom[(a + i) & (chip - 1)];
    return true;
}
bool z5r_link_write_page(uint16_t a, const uint8_t* p) {
    memcpy(eeprom + (a & (chip - 1)), p, Z5R_PAGE);
    last_page_written = a;
    writes++;
    return true;
}

static void fresh(uint8_t first) {
    memset(eeprom, 0xFF, sizeof(eeprom));
    eeprom[0] = first;
    eeprom[5] = 0x15;
    chip = 8192;
    last_page_written = fail_read_at = -1;
    writes = 0;
}

static Z5rDb* db_with(int n) {
    Z5rDb* db = calloc(1, sizeof(*db));
    z5r_init_blank(db, 8192);
    for(int i = 0; i < n; i++) {
        db->keys[i] =
            (Z5rKey){.b = {i & 0xFF, i >> 8, 0, 0, 0, 0}, .len = 6, .attr = Z5rAttrNormal};
    }
    db->count = n;
    return db;
}

/* every type, short and long keys, a full base: build -> parse gives the same keys back */
static void round_trip(uint16_t mem_size) {
    uint16_t n = z5r_capacity(mem_size);
    Z5rDb* db = calloc(1, sizeof(*db));
    z5r_init_blank(db, mem_size);
    for(uint16_t i = 0; i < n; i++) {
        Z5rKey* k = &db->keys[i];
        k->len = i % 3 ? 6 : 3;
        k->attr = 1 + i % 4;
        for(int j = 0; j < 6; j++)
            k->b[j] = (uint8_t)(i * 7 + j * 31);
        k->b[5] &= 0x0F;
        if(k->len == 3) k->b[3] = k->b[4] = k->b[5] = 0;
    }
    db->count = n;
    size_t used = z5r_build(db);
    assert(used <= mem_size && db->mem[used - 1] == 0xE0);
    Z5rDb* back = calloc(1, sizeof(*back));
    memcpy(back->mem, db->mem, sizeof(back->mem));
    assert(z5r_parse(back) && back->count == n && back->mem_size == mem_size);
    for(uint16_t i = 0; i < n; i++) {
        const Z5rKey *a = &db->keys[i], *b = &back->keys[i];
        assert(a->len == b->len && a->attr == b->attr && !memcmp(a->b, b->b, 6));
    }
    free(db);
    free(back);
}

static int parse_dump(const char* path, char* first) {
    static Z5rDb db;
    FILE* f = fopen(path, "rb");
    assert(f);
    assert(fread(db.mem, 1, sizeof(db.mem), f) == 8192);
    fclose(f);
    assert(z5r_parse(&db));
    if(first && db.count) z5r_key_str(&db.keys[0], first, 13);
    return db.count;
}

int main(void) {
    CtlInfo ci;
    Z5rDb* db = db_with(40);

    fresh(0xCF); /* normal controller: commit marker lands last on page 0 */
    assert(z5r_ctl_write(db, &ci, false) == CtlOk);
    assert(last_page_written == 0 && eeprom[0] == 0x33);
    Z5rDb* back = calloc(1, sizeof(*back));
    assert(z5r_ctl_read(back, &ci) == CtlOk && back->count == 40);

    fresh(0xCF); /* read error half way: keys and header of the base in memory stay as they were */
    z5r_init_blank(back, 8192);
    back->mem[3] = 0x12;
    back->count = 7;
    eeprom[3] = 0x99;
    fail_read_at = 0x200;
    assert(z5r_ctl_read(back, &ci) == CtlIo && back->mem[3] == 0x12 && back->count == 7);

    fresh(0x05); /* old layout: nothing written without force */
    assert(z5r_ctl_write(db, &ci, false) == CtlLegacy && writes == 0 && eeprom[0] == 0x05);
    assert(z5r_ctl_write(db, &ci, true) == CtlOk && eeprom[0] == 0x33);

    fresh(0xCF); /* too many keys for 4 KB: refused before any write */
    eeprom[5] = 0x05;
    free(db);
    db = db_with(681);
    assert(z5r_ctl_write(db, &ci, false) == CtlNoFit && writes == 0);

    fresh(0xFF); /* blank 8 KB chip: formatted as 8 KB */
    eeprom[5] = 0xFF;
    assert(
        z5r_ctl_erase(&ci) == CtlOk && eeprom[0] == 0x33 && eeprom[5] == 0x15 &&
        eeprom[11] == 0xFF);
    fresh(0xFF); /* blank 4 KB chip: the alias at 0x1000 gives it away */
    eeprom[5] = 0xFF;
    chip = 4096;
    assert(z5r_ctl_erase(&ci) == CtlOk && eeprom[5] == 0x05);
    fresh(0xCF); /* formatted controller: open time kept */
    eeprom[3] = 0xAB;
    eeprom[4] = 0xCD;
    assert(
        z5r_ctl_erase(&ci) == CtlOk && eeprom[3] == 0xAB && eeprom[4] == 0xCD &&
        eeprom[5] == 0x15);
    fresh(0xCF); /* unknown code: refused, nothing written */
    eeprom[5] = 0x3A;
    assert(z5r_ctl_erase(&ci) == CtlBadCode && writes == 0);

    round_trip(8192);
    round_trip(4096);

    /* legacy 4-byte records from offset 8: flags (1 = master, bit2 = end) + 3-byte key */
    Z5rDb* lg = calloc(1, sizeof(*lg));
    memset(lg->mem, 0xFF, sizeof(lg->mem));
    lg->mem[0] = 0x05;
    lg->mem[5] = 0x15;
    memcpy(lg->mem + 8, (uint8_t[]){1, 0x11, 0x22, 0x33, 0, 0x44, 0x55, 0x66, 4}, 9);
    assert(z5r_parse(lg) && lg->count == 2);
    assert(lg->keys[0].attr == Z5rAttrMaster && lg->keys[0].len == 3 && lg->keys[0].b[2] == 0x33);
    assert(lg->keys[1].attr == Z5rAttrNormal && lg->keys[1].b[0] == 0x44);
    free(lg);

    /* RFID data -> key: EM4100 reversed into a 6-byte key, H10301 FC/number into 3 bytes */
    Z5rKey k;
    char s[13];
    z5r_key_from_rfid(true, (uint8_t[]){0x12, 0x34, 0x56, 0x78, 0x9A}, &k);
    z5r_key_str(&k, s, sizeof(s));
    assert(k.len == 6 && !strcmp(s, "00123456789A"));
    z5r_key_from_rfid(false, (uint8_t[]){0x0A, 0x01, 0x02}, &k);
    z5r_key_str(&k, s, sizeof(s));
    assert(k.len == 3 && !strcmp(s, "0A0102"));

    /* real dumps */
    char first[13];
    assert(
        parse_dump("../../dumps/controller_original.z5r", first) == 1 &&
        !strcmp(first, "00000147F639"));
    assert(parse_dump("../../dumps/basez5r_read.bin", NULL) == 1);
    assert(parse_dump("../../dumps/controller2_original.z5r", NULL) == 3);
    assert(parse_dump("../../dumps/test_1363_keys.bin", NULL) == 1363);

    free(db);
    free(back);
    puts("ctl tests ok");
    return 0;
}
#endif
