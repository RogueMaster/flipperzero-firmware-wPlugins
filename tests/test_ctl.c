/* Host test for z5r_ctl_write with a fake EEPROM (ufbt also compiles this folder, hence the guard):
   gcc -DZ5R_HOST_TEST -I.. test_ctl.c ../z5r_ctl.c ../z5r_format.c -o t && ./t */
#ifdef Z5R_HOST_TEST
#include "../z5r.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t eeprom[8192];
static int last_page_written = -1, writes;
bool z5r_link_present(void) { return true; }
bool z5r_link_read(uint16_t a, uint8_t* b, size_t n) { memcpy(b, eeprom + a, n); return true; }
bool z5r_link_write_page(uint16_t a, const uint8_t* p) {
    memcpy(eeprom + a, p, Z5R_PAGE);
    last_page_written = a;
    writes++;
    return true;
}

static void fresh(uint8_t first) {
    memset(eeprom, 0xFF, sizeof(eeprom));
    eeprom[0] = first;
    eeprom[5] = 0x15;
    last_page_written = -1;
    writes = 0;
}

static Z5rDb* db_with(int n) {
    Z5rDb* db = calloc(1, sizeof(*db));
    z5r_init_blank(db, 8192);
    for(int i = 0; i < n; i++) {
        db->keys[i] = (Z5rKey){.b = {i & 0xFF, i >> 8, 0, 0, 0, 0}, .len = 6, .attr = Z5rAttrNormal};
    }
    db->count = n;
    return db;
}

int main(void) {
    CtlInfo ci;
    Z5rDb* db = db_with(40);

    fresh(0xCF); /* normal controller: commit marker lands last on page 0 */
    assert(z5r_ctl_write(db, &ci, false) == CtlOk);
    assert(last_page_written == 0 && eeprom[0] == 0x33);
    Z5rDb* back = calloc(1, sizeof(*back));
    assert(z5r_ctl_read(back, &ci) == CtlOk && back->count == 40);

    fresh(0x05); /* old layout: nothing written without force */
    assert(z5r_ctl_write(db, &ci, false) == CtlLegacy && writes == 0 && eeprom[0] == 0x05);
    assert(z5r_ctl_write(db, &ci, true) == CtlOk && eeprom[0] == 0x33);

    fresh(0xCF); /* too many keys for 4 KB: refused before any write */
    eeprom[5] = 0x05;
    free(db);
    db = db_with(681);
    assert(z5r_ctl_write(db, &ci, false) == CtlNoFit && writes == 0);

    puts("ctl tests ok");
    return 0;
}
#endif
