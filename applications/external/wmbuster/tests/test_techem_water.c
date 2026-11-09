/* Host-side regression test for the Techem MK-Radio water drivers.
 *
 * Checks that each telegram is routed to the right driver (the MVT
 * lookup) and that the decoded volumes/dates come out right. Vectors
 * come from wmbusmeters `drivers/src/mkradio3.xmq` / `mkradio4.xmq`
 * plus field captures of version 0x52 and 0x95 meters. */

#include "../drivers/engine/driver.h"
#include "../protocol/wmbus_link.h"
#include "../protocol/wmbus_manuf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const WmbusDriver wmbus_drv_techem_mkradio3;
extern const WmbusDriver wmbus_drv_techem_mkradio4;
extern const WmbusDriver wmbus_drv_techem_mkradio4a;

const WmbusDriver* const wmbus_engine_registry[] = {
    &wmbus_drv_techem_mkradio3,
    &wmbus_drv_techem_mkradio4,
    &wmbus_drv_techem_mkradio4a,
};
const size_t wmbus_engine_registry_len = 3;

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

static void
    run(const char* name, const char* hex, const char* driver_id, const char* const* must) {
    uint8_t fr[300];
    size_t flen = hex2bin(hex, fr, sizeof(fr));
    WmbusLinkFrame link;
    if(!wmbus_link_parse(fr, flen, &link)) {
        fprintf(stderr, "%s: link parse failed\n", name);
        exit(1);
    }
    const WmbusDriver* d = wmbus_engine_find(link.M, link.version, link.medium);
    if(strcmp(d->id, driver_id) != 0) {
        fprintf(stderr, "%s: matched driver '%s', expected '%s'\n", name, d->id, driver_id);
        exit(1);
    }
    char out[1024];
    size_t n =
        d->decode(link.M, link.version, link.medium, link.apdu, link.apdu_len, out, sizeof(out));
    out[n < sizeof(out) ? n : sizeof(out) - 1] = 0;
    printf("--- %s [V=%02X T=%02X -> %s] ---\n%s\n", name, link.version, link.medium, d->id, out);
    for(; *must; must++) {
        if(!strstr(out, *must)) {
            fprintf(stderr, "%s: missing expected '%s'\n", name, *must);
            exit(1);
        }
    }
}

int main(void) {
    /* wmbusmeters mkradio3: total 13.8, target 8.9, prev 2018-12-31, current 2019-04-27 */
    run("mkradio3 (wmbusmeters)",
        "2F446850313233347462A2_069F255900B029310000000306060906030609070606050509050505050407040605070500",
        "techem-mkradio3",
        (const char*[]){
            "Techem warm water",
            "Now    13.8 m3",
            "Last   8.9 m3",
            "Reset  2018-12-31",
            "Read   2019-04-27",
            NULL});
    /* wmbusmeters mkradio4: total 0.4, target 0.1 */
    run("mkradio4 (wmbusmeters)",
        "2F446850200141029562A2_06702901006017030004000300000000000000000000000000000000000000000000000000",
        "techem-mkradio4",
        (const char*[]){"Techem warm water", "Now    0.4 m3", "Last   0.1 m3", NULL});
    /* Field capture, version 0x52 warm water */
    run("v0x52 warm (field)",
        "2F446850785634125262A2063E351E003016010000000001010002010102010300000003020000020000000100010000",
        "techem-mkradio3",
        (const char*[]){
            "Techem warm water",
            "Now    3.1 m3",
            "Last   3.0 m3",
            "Reset  2026-09-30",
            "Read   2026-11-03",
            NULL});
    /* Field capture, version 0x52 cold water */
    run("v0x52 cold (field)",
        "2F446850214365875272A2063E35DE003016140000000209100F0E0B0904040303020305060302020004050602040503",
        "techem-mkradio3",
        (const char*[]){
            "Techem cold water", "Now    24.2 m3", "Period 2.0 m3", "Last   22.2 m3", NULL});
    /* Field capture, version 0x95 warm water */
    run("v0x95 warm (field)",
        "2F446850111111119562A2063E35F802301600000000000000000506000406080B0E090B0E0C0A120B0C0E0A0A0E0207",
        "techem-mkradio4",
        (const char*[]){
            "Techem warm water", "Now    76.0 m3", "Reset  2026-09-30", "Read   2026-11-03", NULL});
    printf("\nAll Techem water tests passed.\n");
    return 0;
}
