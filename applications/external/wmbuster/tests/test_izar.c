/* Host-side regression test for the Diehl IZAR / PRIOS driver.
 *
 * Reproduces the wmbusmeters test cases (`_refs/wmbusmeters/src/driver_izar.cc`
 * footer comments) and asserts the same total_m3 values come out.
 * Build with `make -C tests check_izar`. */

#include "../drivers/engine/driver.h"
#include "../protocol/wmbus_link.h"
#include "../protocol/wmbus_manuf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern const WmbusDriver wmbus_drv_diehl_izar;

const WmbusDriver* const wmbus_engine_registry[] = { &wmbus_drv_diehl_izar };
const size_t wmbus_engine_registry_len = 1;

static int hex2nyb(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static size_t hex2bin(const char* s, uint8_t* out, size_t cap) {
    size_t n = 0;
    while(*s && n < cap) {
        if(*s == ' ' || *s == '|' || *s == '_') { s++; continue; }
        int hi = hex2nyb(*s++); if(hi < 0) break;
        int lo = hex2nyb(*s++); if(lo < 0) break;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    return n;
}

static void run(const char* name, const char* hex, const char* must_contain) {
    uint8_t fr[300];
    size_t  flen = hex2bin(hex, fr, sizeof(fr));
    WmbusLinkFrame link;
    if(!wmbus_link_parse(fr, flen, &link)) {
        fprintf(stderr, "%s: link parse failed\n", name);
        exit(1);
    }
    WmbusDecodeCtx ctx = {
        .manuf = link.M, .version = link.version, .medium = link.medium,
        .ci = link.ci, .id = link.id,
        .apdu = link.apdu, .apdu_len = link.apdu_len,
    };
    char out[1024];
    size_t n = wmbus_drv_diehl_izar.decode_ex(&ctx, out, sizeof(out));
    out[n < sizeof(out) ? n : sizeof(out) - 1] = 0;

    char mc[4]; wmbus_manuf_decode(link.M, mc);
    printf("--- %s [M=%s V=%02X T=%02X CI=%02X ID=%08X] ---\n%s\n",
           name, mc, link.version, link.medium, link.ci, (unsigned)link.id, out);

    if(must_contain && !strstr(out, must_contain)) {
        fprintf(stderr, "%s: missing expected substring '%s'\n",
                name, must_contain);
        exit(1);
    }
}

int main(void) {
    /* Tests lifted verbatim from driver_izar.cc — telegram + expected
     * total_m3 substring. wmbusmeters's "NOKEY" tests use the public
     * default keys, which is exactly what our default-key path tries. */
    run("IzarWater",
        "1944304C72242421D401A2 013D4013DD8B46A4999C1293E582CC",
        "3.488 m3");
    run("IzarWater3",
        "1944A5117807791948 20A1 21170013355F8EDB2D03C6912B1E37",
        "4.366 m3");
    run("IzarWater4",
        "1944304c9c5824210c04 a3 63140013716577ec59e8663ab0d31c",
        "38.944 m3");
    run("IzarWater5",
        "1944304CDEFFE420CC01 A2 63120013258F907B0AFF12529AC33B",
        "159.832 m3");
    run("IzarWater6",
        "194424238607750350 48 A251520015BEB6B2E1ED623A18FC74A5",
        "521.602 m3");
    printf("\nAll IZAR tests passed.\n");
    return 0;
}
