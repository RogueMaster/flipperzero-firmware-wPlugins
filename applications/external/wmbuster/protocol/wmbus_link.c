#include "wmbus_link.h"
#include "wmbus_manuf.h"
#include "wmbus_medium.h"
#include <stdio.h>
#include <string.h>

/* CI codes that carry an encryption configuration word. We support the most
 * common short and long headers per OMS Vol.2. */
static void parse_encryption(const WmbusLinkFrame* f_in, WmbusLinkFrame* f_out) {
    f_out->enc_mode = 0;
    f_out->access_no = 0;
    if(f_in->apdu_len < 4) return;
    const uint8_t* p = f_in->apdu;

    /* Short header: AccessNo(1) Status(1) Conf(2) ... */
    /* Long header : ID(4) Ver(1) Med(1) AccessNo(1) Status(1) Conf(2) ... */
    size_t off = 0;
    switch(f_in->ci) {
    case 0x7A: /* short header, response (meter -> other) */
    case 0x5A: /* short, command */
        off = 0;
        break;
    case 0x72: /* long header */
    case 0x53:
    case 0x8B:
        off = 6;
        break;
    default:
        return;
    }
    if(f_in->apdu_len < off + 4) return;
    f_out->access_no = p[off];
    uint16_t conf = (uint16_t)(p[off + 2] | (p[off + 3] << 8));
    /* Encryption mode lives in bits 12..8 of the Configuration Word (per
     * OMS Vol.2 §7.2.5 + EN 13757-3 §6.4 — see also wmbusmeters
     * `meters_common_implementation.cc` which uses `(cw & 0x1F00) >> 8`).
     *
     * Earlier we used `(conf >> 12) & 0x07` which sliced the WRONG bits
     * and returned mode 2 (DES-CBC, deprecated) for every Techem v0x6A
     * frame in the wild. Real value is 5 (AES-CBC static IV) → those
     * frames now flow into the AES decrypt path correctly when a key
     * is on file. Verified against rtl_433-captured Techem traffic in
     * a residential building (CW = 0x2510 → mode 5).                  */
    f_out->enc_mode = (uint8_t)((conf >> 8) & 0x1F);
}

bool wmbus_link_parse(const uint8_t* fr, size_t len, WmbusLinkFrame* out) {
    if(len < 10) return false;
    memset(out, 0, sizeof(*out));
    out->L = fr[0];
    out->C = fr[1];
    out->M = (uint16_t)(fr[2] | (fr[3] << 8));
    out->id = (uint32_t)(fr[4] | (fr[5] << 8) | (fr[6] << 16) | (fr[7] << 24));
    out->version = fr[8];
    out->medium = fr[9];

    if(len < 11) {
        out->ci = 0;
        out->apdu = NULL;
        out->apdu_len = 0;
        return true;
    }
    out->ci = fr[10];
    out->apdu = &fr[11];
    out->apdu_len = len - 11;

    parse_encryption(out, out);
    return true;
}

void wmbus_link_to_str(const WmbusLinkFrame* f, char* out, size_t out_size) {
    char m[4]; wmbus_manuf_decode(f->M, m);
    const char* mn = wmbus_manuf_name(f->M);
    snprintf(
        out, out_size,
        "%s %08lX v%02X %s C=%02X CI=%02X enc=%u%s%s",
        m, (unsigned long)f->id, f->version, wmbus_medium_str(f->medium),
        f->C, f->ci, f->enc_mode,
        mn ? " (" : "", mn ? mn : "");
}
