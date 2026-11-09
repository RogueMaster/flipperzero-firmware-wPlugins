/* Host-side regression test for the Techem heat, HCA and smoke drivers.
 *
 * Checks routing (MVT lookup) and decoded values against the test
 * vectors in wmbusmeters `drivers/src/{compact5,vario451,vario411,
 * fhkvdataiii,tsd2}.xmq`, plus a field capture of an FHKV HCA. */

#include "../drivers/engine/driver.h"
#include "../protocol/wmbus_link.h"
#include "../protocol/wmbus_manuf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const WmbusDriver wmbus_drv_techem_fhkv;
extern const WmbusDriver wmbus_drv_techem_compact5;
extern const WmbusDriver wmbus_drv_techem_vario;
extern const WmbusDriver wmbus_drv_techem_vario_oms;
extern const WmbusDriver wmbus_drv_techem_smoke;
extern const WmbusDriver wmbus_drv_techem_tsd2;

const WmbusDriver* const wmbus_engine_registry[] = {
    &wmbus_drv_techem_fhkv,
    &wmbus_drv_techem_compact5,
    &wmbus_drv_techem_vario,
    &wmbus_drv_techem_vario_oms,
    &wmbus_drv_techem_smoke,
    &wmbus_drv_techem_tsd2,
};
const size_t wmbus_engine_registry_len = 6;

static int hex2nyb(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static size_t hex2bin(const char* s, uint8_t* out, size_t cap) {
    size_t n = 0;
    while(*s && n < cap) {
        if(*s == ' ' || *s == '_') {
            s++;
            continue;
        }
        int hi = hex2nyb(*s++);
        if(hi < 0) break;
        int lo = hex2nyb(*s++);
        if(lo < 0) break;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    return n;
}

/* Routing check only: header bytes are enough. */
static void route(const char* name, const char* hex, const char* driver_id) {
    uint8_t fr[16];
    hex2bin(hex, fr, sizeof(fr));
    uint16_t m = (uint16_t)(fr[2] | (fr[3] << 8));
    const WmbusDriver* d = wmbus_engine_find(m, fr[8], fr[9]);
    printf("--- %s [V=%02X T=%02X -> %s]\n", name, fr[8], fr[9], d->id);
    if(strcmp(d->id, driver_id) != 0) {
        fprintf(stderr, "%s: matched '%s', expected '%s'\n", name, d->id, driver_id);
        exit(1);
    }
}

static void
    run(const char* name, const char* hex, const char* driver_id, const char* const* must) {
    uint8_t fr[300];
    size_t flen = hex2bin(hex, fr, sizeof(fr));
    WmbusLinkFrame link;
    if(!wmbus_link_parse(fr, flen, &link)) {
        fprintf(stderr, "%s: link parse failed\n", name);
        exit(1);
    }
    route(name, hex, driver_id);
    const WmbusDriver* d = wmbus_engine_find(link.M, link.version, link.medium);
    char out[1024];
    size_t n =
        d->decode(link.M, link.version, link.medium, link.apdu, link.apdu_len, out, sizeof(out));
    out[n < sizeof(out) ? n : sizeof(out) - 1] = 0;
    printf("%s\n", out);
    for(; *must; must++) {
        if(!strstr(out, *must)) {
            fprintf(stderr, "%s: missing expected '%s'\n", name, *must);
            exit(1);
        }
    }
}

int main(void) {
    /* compact5: previous 375, current 120, total 495 kWh */
    run("compact5 (wmbusmeters)",
        "36446850626262624543A1_009F2777010060780000000A000000000000000000000000000000000000000000000000A0400000B4010000",
        "techem-compact5",
        (const char*[]){"Total  495 kWh", "Period 120 kWh", "Last   375 kWh", NULL});
    /* vario451: previous 3642.2, current 2729.4 kWh */
    run("vario451 (wmbusmeters)",
        "374468506549235827C3A2_129F25383300A8622600008200800A2AF862115175552877A36F26C9AB1CB24400000004000000000004908002",
        "techem-vario",
        (const char*[]){"Period 2729 kWh", "Last   3642 kWh", NULL});
    /* vario411 / vario451mid: OMS frames, only routing is checked */
    route("vario411 (wmbusmeters)", "624468507578626728048C00", "techem-vario-oms");
    route("vario451mid (wmbusmeters)", "734468501204439417048c00", "techem-vario-oms");
    /* fhkvdataiii: current 131, previous 1026, room 22.44 C, radiator 25.51 C */
    run("fhkvdataiii 0x69 (wmbusmeters)",
        "34446850226677116980A0119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000",
        "techem-fhkv",
        (const char*[]){
            "Now    131 u",
            "Prev   1026 u",
            "Reset  2019-12-31",
            "Ends   2020-02-08",
            "Room   22.44 C",
            "Rad    25.51 C",
            NULL});
    run("fhkvdataiii 0x94 (wmbusmeters)",
        "33446850341211119480A2_0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E",
        "techem-fhkv",
        (const char*[]){"Now    4 u", "Prev   45 u", "Room   22.37 C", "Rad    23.60 C", NULL});
    /* tsd2: status OK / SMOKE / DEFECT, previous date 2019-12-31 */
    run("tsd2 ok (wmbusmeters)",
        "294468506935639176F0A0_009F2782290060822900000401D6311AF93E1BF93E008DC3009ED4000FE500",
        "techem-tsd2",
        (const char*[]){"Status OK", "Reset  2019-12-31", NULL});
    run("tsd2 smoke (wmbusmeters)",
        "294468506935639176F0A0_019F2782290060822900000401D6311AF93E1BF93E008DC3009ED4000FE500",
        "techem-tsd2",
        (const char*[]){"Status SMOKE", NULL});
    run("tsd2 defect (wmbusmeters)",
        "294468506935639176F0A0_809F2782290060822900000401D6311AF93E1BF93E008DC3009ED4000FE580",
        "techem-tsd2",
        (const char*[]){"Status DEFECT", NULL});
    printf("\nAll Techem heat/HCA/smoke tests passed.\n");
    return 0;
}
