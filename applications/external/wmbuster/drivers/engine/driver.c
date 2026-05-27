/* wM-Buster driver engine — implementation.
 *
 * The matching algorithm is a single pass over the hand-curated
 * registry. We test each driver's MVT list in order; the first
 * triple match wins. Wildcards (0xFF in version or medium) match
 * anything. There is no priority ordering beyond registry order
 * — `drivers/engine/registry.c` is curated such that the most
 * specific drivers come first and brand-wide fallbacks come last.
 *
 * Why no smarter matching? With ~250 entries a linear scan costs
 * ~50 µs on the F7, dwarfed by the AES decryption and SD-log
 * write that happen on the same telegram. A hash table would save
 * microseconds we'll never observe.
 */

#include "driver.h"
#include "../../protocol/wmbus_app_layer.h"
#include "../../protocol/wmbus_manuf.h"
#include "../../protocol/wmbus_medium.h"

#include <stdio.h>
#include <string.h>

static bool mvt_matches(const WmbusMVT* m,
                        const char* code, uint8_t v, uint8_t med) {
    if(memcmp(m->manuf, code, 3) != 0) return false;
    if(m->version != 0xFF && m->version != v)   return false;
    if(m->medium  != 0xFF && m->medium  != med) return false;
    return true;
}

static const WmbusDriver* find_in_registry(
    uint16_t manuf, uint8_t version, uint8_t medium) {
    char code[4]; wmbus_manuf_decode(manuf, code);
    for(size_t i = 0; i < wmbus_engine_registry_len; i++) {
        const WmbusDriver* d = wmbus_engine_registry[i];
        if(!d || !d->mvt) continue;
        for(const WmbusMVT* m = d->mvt; m->manuf[0]; m++) {
            if(mvt_matches(m, code, version, medium)) return d;
        }
    }
    return NULL;
}

/* Sentinel driver used when nothing in the registry matches. We build a
 * descriptive title from the (manuf, medium, version) triple so the user
 * can see exactly which device was received even when no registered driver
 * claims it — and so the walker-failure fallback reads naturally. */
static size_t decode_unknown_oms(uint16_t m, uint8_t v, uint8_t med,
                                 const uint8_t* a, size_t l,
                                 char* o, size_t cap) {
    char code[4]; wmbus_manuf_decode(m, code);
    const char* med_str = wmbus_medium_str(med);
    char title[48];
    snprintf(title, sizeof(title), "%s %s v0x%02X", code, med_str, v);
    return wmbus_engine_render_oms(title, a, l, o, cap);
}
static const WmbusMVT k_unknown_mvt[] = { { {0}, 0, 0 } };
static const WmbusDriver k_unknown_driver = {
    .id = "unknown-oms",
    .title = "OMS payload",
    .mvt = k_unknown_mvt,
    .decode = decode_unknown_oms,
};

const WmbusDriver* wmbus_engine_find(
    uint16_t manuf, uint8_t version, uint8_t medium) {
    const WmbusDriver* d = find_in_registry(manuf, version, medium);
    return d ? d : &k_unknown_driver;
}

size_t wmbus_engine_decode(
    uint16_t manuf, uint8_t version, uint8_t medium,
    const uint8_t* apdu, size_t apdu_len,
    char* out, size_t cap) {
    /* Legacy entry: synthesise a context with no meter-ID / CI info. Drivers
     * that genuinely need those (PRIOS) opt in via decode_ex and treat a
     * zero ctx as "abort, not enough information". */
    WmbusDecodeCtx ctx = {
        .manuf = manuf, .version = version, .medium = medium,
        .ci = 0, .id = 0,
        .apdu = apdu, .apdu_len = apdu_len,
    };
    return wmbus_engine_decode_ctx(&ctx, out, cap);
}

size_t wmbus_engine_decode_ctx(const WmbusDecodeCtx* ctx,
                               char* out, size_t cap) {
    const WmbusDriver* d = wmbus_engine_find(ctx->manuf, ctx->version, ctx->medium);
    if(d->decode_ex) {
        return d->decode_ex(ctx, out, cap);
    }
    if(d->decode) {
        return d->decode(ctx->manuf, ctx->version, ctx->medium,
                         ctx->apdu, ctx->apdu_len, out, cap);
    }
    /* No custom decode → render with title + OMS walker. */
    return wmbus_engine_render_oms(d->title, ctx->apdu, ctx->apdu_len, out, cap);
}

const char* wmbus_engine_model_name(
    uint16_t manuf, uint8_t version, uint8_t medium) {
    const WmbusDriver* d = find_in_registry(manuf, version, medium);
    return d ? d->title : NULL;
}

size_t wmbus_engine_render_oms(
    const char* title,
    const uint8_t* apdu, size_t apdu_len,
    char* out, size_t cap) {
    if(!out || cap == 0) return 0;
    size_t pos = (size_t)snprintf(out, cap, "%s\n", title ? title : "OMS payload");
    if(pos >= cap) return pos;

    size_t walker = wmbus_app_render(apdu, apdu_len, out + pos, cap - pos);
    pos += walker;
    if(walker > 0) return pos;

    /* Walker emitted nothing. Render a clean label/value pair list — the
     * detail-view contract guarantees these render as a perfectly aligned
     * "label  value" two-column row each, and the right column never
     * overflows because every value below stays under 16 chars. The hex
     * preview gives a developer the first 32 bytes (4 rows × 8 bytes) so
     * an unknown frame can still be reverse-engineered from a screenshot. */
    /* `Bytes NN` is line 2 by design — wmbus_app.c lifts that line into
     * the scan-list summary, so the user sees a useful "123 bytes" hint
     * instead of a stuttering "Status no d…" string. The actual "no
     * decoder" status follows on line 3 inside the detail screen. */
    pos += (size_t)snprintf(out + pos, cap - pos,
                            "Bytes  %u\n"
                            "Status  no decoder\n"
                            "Repo  i12bp8/wmbuster\n",
                            (unsigned)apdu_len);
    size_t off = 0;
    int rows = 0;
    while(off < apdu_len && rows < 4 && pos + 24 < cap) {
        size_t n = apdu_len - off; if(n > 6) n = 6;
        /* Label "Hex 00" / "Hex 06" / ... and a 12-hex-char value. The
         * 6-byte chunk fits the value column (≤16 chars) on every row. */
        pos += (size_t)snprintf(out + pos, cap - pos, "Hex%02u ", (unsigned)off);
        for(size_t k = 0; k < n && pos + 3 < cap; k++)
            pos += (size_t)snprintf(out + pos, cap - pos, "%02X", apdu[off + k]);
        if(pos + 1 < cap) { out[pos++] = '\n'; out[pos] = 0; }
        off += n;
        rows++;
    }
    return pos;
}
