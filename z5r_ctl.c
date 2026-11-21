#include "z5r.h"
#include <string.h>

CtlResult z5r_ctl_probe(uint8_t page0[Z5R_PAGE], CtlInfo* i) {
    memset(i, 0, sizeof(*i));
    if(!z5r_link_present() || !z5r_link_read(0, page0, Z5R_PAGE)) return CtlNoLink;
    i->code = page0[5];
    i->size = z5r_mem_size_from_code(i->code);
    return CtlOk;
}

CtlResult z5r_ctl_read(Z5rDb* db, CtlInfo* i) {
    uint8_t p0[Z5R_PAGE];
    CtlResult r = z5r_ctl_probe(p0, i);
    if(r) return r;
    if(!i->size) return CtlBadCode;
    for(uint16_t a = 0; a < i->size; a += 256)
        if(!z5r_link_read(a, db->mem + a, 256)) return CtlIo;
    return z5r_parse(db) ? CtlOk : CtlBadCode;
}

CtlResult z5r_ctl_write(Z5rDb* db, CtlInfo* i, bool force) {
    uint8_t p0[Z5R_PAGE];
    CtlResult r = z5r_ctl_probe(p0, i);
    if(r) return r;
    if(!i->size) return CtlBadCode;
    if(db->count > z5r_capacity(i->size)) return CtlNoFit;
    /* the build always emits the new layout; an old firmware may not understand it */
    if(p0[0] < 0x11 && !force) return CtlLegacy;
    db->mem_size = i->size; /* image layout of this controller */
    /* an empty base is fine: only the open time may be needed, the controller then collects keys by itself */
    memcpy(db->mem + 1, p0 + 1, 2); /* keep controller-specific header bytes, as BaseZ5R does */
    size_t used = z5r_build(db);
    for(size_t a = 0; a < used; a += Z5R_PAGE) {
        uint8_t cur[Z5R_PAGE];
        /* only changed pages go out; page 0 always (it carries the in-progress marker) */
        if(a && z5r_link_read(a, cur, Z5R_PAGE) && !memcmp(cur, db->mem + a, Z5R_PAGE)) continue;
        if(!z5r_link_write_page(a, db->mem + a)) {
            i->addr = a;
            return CtlPageFail;
        }
    }
    db->mem[0] = 0x33; /* commit marker last: controller ignores a half-written table */
    return z5r_link_write_page(0, db->mem) ? CtlOk : CtlCommitFail;
}

CtlResult z5r_ctl_erase(CtlInfo* i) {
    uint8_t p0[Z5R_PAGE];
    CtlResult r = z5r_ctl_probe(p0, i);
    if(r) return r;
    uint16_t size = i->size;
    if(!size) {
        /* only a blank chip (0xFF) may be formatted as 8 KB; 0x3A/0x7A etc. is a bigger "Z7" controller */
        if(p0[5] != 0xFF) return CtlBadCode;
        size = 8192; /* blank chip: standard Z-5R (1363 keys) */
    }
    uint8_t keep2 = p0[2], keep3 = p0[3], keep4 = p0[4];
    bool fmt = i->size != 0;
    memset(p0, 0xFF, sizeof(p0));
    p0[0] = 0x33;
    p0[1] = 0x00;
    p0[2] = keep2;
    p0[3] = fmt ? keep3 : 0xFF;
    p0[4] = fmt ? keep4 : 0xD1;
    p0[5] = size == 4096 ? 0x05 : 0x15;
    if(!z5r_link_write_page(0, p0)) {
        i->addr = 0;
        return CtlPageFail;
    }
    return CtlOk;
}
