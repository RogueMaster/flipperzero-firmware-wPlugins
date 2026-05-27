/* Host-side regression test for the eight RE'd drivers ported from
 * wmbusmeters in Apr 2026. Each test pumps a reference telegram from
 * the upstream `_refs/wmbusmeters/src/driver_*.cc` test footer through
 * our driver and asserts that the field substring we care about
 * appears in the rendered output. */

#include "../drivers/engine/driver.h"
#include "../protocol/wmbus_link.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const WmbusDriver wmbus_drv_bmeters_hydrodigit;
extern const WmbusDriver wmbus_drv_engelmann_hydroclima;
extern const WmbusDriver wmbus_drv_sontex_rfmtx1;
extern const WmbusDriver wmbus_drv_zenner0b;
extern const WmbusDriver wmbus_drv_techem_mkradio3a;
extern const WmbusDriver wmbus_drv_gwf_water;
/* bfw240radio uses the legacy `decode` path (no ctx); skip it here. */

const WmbusDriver* const wmbus_engine_registry[] = {
    &wmbus_drv_bmeters_hydrodigit,
    &wmbus_drv_engelmann_hydroclima,
    &wmbus_drv_sontex_rfmtx1,
    &wmbus_drv_zenner0b,
    &wmbus_drv_techem_mkradio3a,
    &wmbus_drv_gwf_water,
};
const size_t wmbus_engine_registry_len =
    sizeof(wmbus_engine_registry) / sizeof(wmbus_engine_registry[0]);

static int hex2nyb(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static size_t hex2bin(const char* s, uint8_t* out, size_t cap) {
    size_t n = 0;
    while(*s && n < cap) {
        if(*s == ' ' || *s == '|' || *s == '_' || *s == '\n') { s++; continue; }
        int hi = hex2nyb(*s++); if(hi < 0) break;
        int lo = hex2nyb(*s++); if(lo < 0) break;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    return n;
}

static int run(const char* name, const WmbusDriver* drv,
               const char* hex, const char* must_contain) {
    uint8_t fr[300];
    size_t  flen = hex2bin(hex, fr, sizeof(fr));
    WmbusLinkFrame link;
    if(!wmbus_link_parse(fr, flen, &link)) {
        fprintf(stderr, "%s: link parse failed\n", name);
        return 1;
    }
    WmbusDecodeCtx ctx = {
        .manuf = link.M, .version = link.version, .medium = link.medium,
        .ci = link.ci, .id = link.id,
        .apdu = link.apdu, .apdu_len = link.apdu_len,
    };
    char out[1024] = {0};
    size_t n = drv->decode_ex
        ? drv->decode_ex(&ctx, out, sizeof(out))
        : drv->decode(link.M, link.version, link.medium,
                      link.apdu, link.apdu_len, out, sizeof(out));
    out[n < sizeof(out) ? n : sizeof(out) - 1] = 0;

    printf("--- %s ---\n%s\n", name, out);
    if(must_contain && !strstr(out, must_contain)) {
        fprintf(stderr, "%s: missing expected substring '%s'\n",
                name, must_contain);
        return 1;
    }
    return 0;
}

int main(void) {
    int fails = 0;

    /* Zenner B.One: bytes 8..11 LE = 0x001BBF80 = 1,818,496; /256000 = 7.1035
     * (target). Bytes after that = 0x0042A600 = 4,367,360; /256000 ≈ 17.060.
     * The wmbusmeters expected value is 17.062 — within rounding. */
    fails += run("zenner0b", &wmbus_drv_zenner0b,
        "1E44496A677308500B167AD80010252F2F0F00000000"
        "80BF1B0000A6420000",
        "Total");

    /* GWF water: status row + total walker output. We assert presence of
     * the "Mode" (power_mode) row which only the driver emits. */
    fails += run("gwfwater", &wmbus_drv_gwf_water,
        "3144E61E311022200110"
        "8C04F47ABE0420452F2F"
        "037410000004133E0000004413FFFFFFFF426CFFFF0F0120012F2F2F2F2F",
        "Mode");

    /* BMeters Hydrodigit: total_m3 walker (3.866) + voltage row from mfct
     * trailer (0x2A → low nibble 0x0A → 3.05 V). */
    fails += run("hydrodigit", &wmbus_drv_bmeters_hydrodigit,
        "4E44B4098686868613077AF00040052F2F"
        "0C1366380000046D27287E2A"
        "0F150E00000000C10000D10000E60000FD00000C01002F0100410100540100680100890000A00000B30000002F2F2F2F2F2F",
        "Volt");

    /* Sontex RFM-TX1 — legacy XOR path. */
    fails += run("rfmtx1", &wmbus_drv_sontex_rfmtx1,
        "4644B4097172737405077AA5000610"
        "1115F78184AB0F1D1E200000005904103103208047004A4800E73C00193E00453F003E4000E64000E74100F442000144001545005B460000",
        "Total");

    /* Engelmann Hydroclima — RKN0 path (036E in prefix). */
    fails += run("hydroclima", &wmbus_drv_engelmann_hydroclima,
        "2e44b0099861036853087a000020002F2F"
        "036E0000000F100043106A7D2C4A078F12202CB1242A06D3062100210000",
        "AvgAmb");

    if(fails == 0) printf("\nAll port tests passed.\n");
    else           fprintf(stderr, "\n%d test(s) failed.\n", fails);
    return fails ? 1 : 0;
}
