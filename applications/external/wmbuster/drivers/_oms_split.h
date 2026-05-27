/* Shared helper for proprietary-tail drivers.
 *
 * A common pattern for European water/heat meters that ship custom
 * frame extensions: the front of the APDU is a normal OMS DIF/VIF
 * stream (parseable by `wmbus_app_render`) and the moment the parser
 * encounters DIF=0x0F or 0x1F it stops because the rest is
 * manufacturer-specific.
 *
 * Drivers in this category — bmeters/hydrodigit, engelmann/hydroclima,
 * misc/gwfwater, zenner/zenner0b, etc. — all want the same flow:
 *
 *     write title
 *     render OMS prefix up to (but not including) the 0F marker
 *     parse the trailer themselves and append rows
 *
 * This header packs that into one inline call so every driver just
 * does `oms_split_emit(ctx, out, cap, &mfct, &mfct_len)` and then
 * appends its own custom rows. */

#pragma once

#include "engine/driver.h"
#include "../protocol/wmbus_app_layer.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Locate the manufacturer-specific marker (DIF 0x0F / 0x1F).
 * Returns the *index of the marker byte* or `len` if none found. */
static inline size_t oms_split_find_mfct(const uint8_t* a, size_t len) {
    for(size_t i = 0; i < len; i++) {
        if(a[i] == 0x0F || a[i] == 0x1F) return i;
    }
    return len;
}

/* Number of header bytes that follow the CI byte before the DIF/VIF
 * stream actually begins. wmbus_app_render expects DIF/VIF input, so we
 * have to step over these manually. Mirrors the table in `wmbus_app.c`
 * used for AES IV construction so the two paths agree. */
static inline size_t oms_split_header_len(uint8_t ci) {
    switch(ci) {
        case 0x7A: case 0x5A:                       return 4;   /* short header  */
        case 0x72: case 0x53: case 0x8B:            return 12;  /* long header   */
        default:                                    return 0;   /* proprietary   */
    }
}

/* Write the driver title, walk the OMS prefix, and hand back a pointer
 * to the manufacturer-specific tail (caller free to ignore). All output
 * goes into `out[0..cap)`; the function returns the number of bytes
 * written. The split index is returned via `*mfct_off` (set to len when
 * absent). The CI byte from the link header is mandatory — without it
 * we cannot strip the OMS short/long header that precedes the DIF/VIF
 * stream and the walker would emit garbage records for the header bytes. */
static inline size_t oms_split_emit(
    const char* title, uint8_t ci,
    const uint8_t* apdu, size_t apdu_len,
    char* out, size_t cap,
    const uint8_t** mfct_out, size_t* mfct_len_out, size_t* mfct_off_out) {

    if(!out || cap == 0) return 0;
    size_t pos = (size_t)snprintf(out, cap, "%s\n", title ? title : "Meter");
    if(pos >= cap) return pos;

    /* Step over the OMS header so the DIF/VIF walker sees real DIFs. */
    size_t hdr = oms_split_header_len(ci);
    if(hdr >= apdu_len) {
        if(mfct_off_out) *mfct_off_out = apdu_len;
        if(mfct_out)     *mfct_out     = NULL;
        if(mfct_len_out) *mfct_len_out = 0;
        return pos;
    }
    const uint8_t* body  = apdu + hdr;
    size_t         blen  = apdu_len - hdr;

    size_t cut = oms_split_find_mfct(body, blen);
    pos += wmbus_app_render(body, cut, out + pos, cap - pos);

    if(mfct_off_out) *mfct_off_out = cut + hdr; /* in original-apdu coords */
    if(cut + 1 < blen) {
        if(mfct_out)     *mfct_out     = body + cut + 1;
        if(mfct_len_out) *mfct_len_out = blen - cut - 1;
    } else {
        if(mfct_out)     *mfct_out     = NULL;
        if(mfct_len_out) *mfct_len_out = 0;
    }
    return pos;
}
