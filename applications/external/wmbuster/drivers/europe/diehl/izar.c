/* Diehl IZAR / PRIOS water meter driver — port of
 * `_refs/wmbusmeters/src/driver_izar.cc` (Jacek Tomasiak / Fredrik Öhrström,
 * GPL-3.0+).
 *
 * IZAR meters wrap their payload in a custom *non-OMS* obfuscation: a
 * 32-bit Galois LFSR keystream XOR'd over the bytes after the first four
 * payload bytes. The seed mixes part of the link header (manuf, ID,
 * version, medium, CI byte) with a 32-bit derivation of one of two
 * publicly known "default" keys, so most field-deployed meters can be
 * read without per-device keys.
 *
 * The seed mixing reproduces wmbusmeters' `decodeDiehlLfsr` byte-for-byte:
 *
 *     key ^= u32be(M_lsb, M_msb,  ID0,  ID1)
 *     key ^= u32be(ID2,    ID3,   Ver,  Med)
 *     key ^= u32be(CI,     A0,    A1,   A2)
 *
 * where ID is little-endian on the wire (`link.id` already is) and A0..A2
 * are the first three bytes of the APDU after CI. The two default keys
 * are XOR-folded halves of `PRIOS_DEFAULT_KEY1/2`. After running the
 * keystream over the encrypted block we sanity-check the first decoded
 * byte against 0x4B (`HEADER_1_BYTE` method); a wrong key produces a
 * different value because the keystream is keyed on the seed.
 *
 * Once decoded the body holds:
 *
 *     [0]              header byte (0x4B sanity)
 *     [1..4]           total water consumption, little-endian liters
 *     [5..8]           previous-month total, little-endian liters
 *     [9..10]          date of previous-month reading
 *
 * The SAP-branded variant additionally encodes a manufacture year +
 * serial number + supplier/diameter/type letters into the link header
 * (origin[4..9]); we surface those as the human-readable "Prefix" and
 * "Serial" rows so a field tech can match the screen against the
 * sticker on the meter.
 *
 * This driver registers under `decode_ex` (engine extension) so it can
 * read the meter ID and CI byte that PRIOS needs but the legacy
 * decode signature does not provide. */

#include "../../engine/driver.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

/* PRIOS default keys (publicly published by wmbusmeters). XOR-folded down
 * to the 32-bit LFSR seed exactly like `convertKey()` in
 * manufacturer_specificities.cc. */
static const uint32_t k_prios_default_keys[] = {
    /* 39BC8A10 ^ E66D83F8 = DFD109E8 */
    0xDFD109E8u,
    /* 51728910 ^ E66D83F8 = B71F0AE8 */
    0xB71F0AE8u,
};

/* SAP manufacturer code packed into the wM-Bus 16-bit field. The packing
 * is 5 bits per letter (A=1) bigendian into bits 14..10, 9..5, 4..0 — for
 * "SAP" that yields 0x4C30 (we hard-code rather than depend on the
 * encoding helper to keep this driver self-contained). */
#define MANUF_SAP 0x4C30u

static uint32_t u32be(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8)  | (uint32_t)b3;
}

static uint32_t u32le_at(const uint8_t* p) {
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8)  | (uint32_t)p[0];
}

/* Run the PRIOS Galois LFSR keystream over `enc` of length `n` into `dec`.
 * The LFSR is seeded with `seed` and clocked 8 times per output byte; the
 * tap polynomial matches the public reverse-engineering. Returns true if
 * the decoded block passes the HEADER_1_BYTE check (first byte == 0x4B). */
static bool prios_decrypt(uint32_t seed,
                          const uint8_t* enc, size_t n,
                          uint8_t* dec) {
    uint32_t key = seed;
    for(size_t i = 0; i < n; i++) {
        for(int j = 0; j < 8; j++) {
            uint8_t bit =
                ((key & 0x00000002u) != 0) ^
                ((key & 0x00000004u) != 0) ^
                ((key & 0x00000800u) != 0) ^
                ((key & 0x80000000u) != 0);
            key = (key << 1) | bit;
        }
        dec[i] = enc[i] ^ (uint8_t)(key & 0xFF);
        if(i == 0 && dec[0] != 0x4B) return false; /* fast path */
    }
    return n > 0 && dec[0] == 0x4B;
}

/* Build the seed-mix word from the link header + first 3 APDU bytes,
 * mirroring `decodeDiehlLfsr` exactly. */
static uint32_t prios_seed(uint32_t base_key,
                           const WmbusDecodeCtx* c) {
    uint8_t id0 = (uint8_t)(c->id);
    uint8_t id1 = (uint8_t)(c->id >> 8);
    uint8_t id2 = (uint8_t)(c->id >> 16);
    uint8_t id3 = (uint8_t)(c->id >> 24);
    uint8_t m_lsb = (uint8_t)(c->manuf);
    uint8_t m_msb = (uint8_t)(c->manuf >> 8);

    uint32_t key = base_key;
    key ^= u32be(m_lsb, m_msb, id0, id1);
    key ^= u32be(id2, id3, c->version, c->medium);
    key ^= u32be(c->ci, c->apdu[0], c->apdu[1], c->apdu[2]);
    return key;
}

/* Append a label/value row, respecting the 11/19-char width budget that
 * `views/detail_canvas.c` enforces. Returns updated cursor. */
static size_t row(char* o, size_t cap, size_t pos,
                  const char* label, const char* fmt, ...) {
    if(pos >= cap) return pos;
    int n = snprintf(o + pos, cap - pos, "%s ", label);
    if(n < 0) return pos;
    pos += (size_t)n;
    if(pos >= cap) return cap;

    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(o + pos, cap - pos, fmt, ap);
    va_end(ap);
    if(n < 0) return pos;
    pos += (size_t)n;
    if(pos < cap) o[pos++] = '\n';
    if(pos < cap) o[pos] = 0;
    return pos;
}

/* Render the SAP-PRIOS prefix + serial + manufacture year. The byte layout
 * lives in the *link header* (origin[4..9]) which we re-synthesise from
 * the parsed context — see the file header comment for the field map. */
static size_t render_sap_prefix(char* o, size_t cap, size_t pos,
                                const WmbusDecodeCtx* c) {
    uint8_t id0 = (uint8_t)(c->id);
    uint8_t id1 = (uint8_t)(c->id >> 8);
    uint8_t id2 = (uint8_t)(c->id >> 16);
    uint8_t id3 = (uint8_t)(c->id >> 24);

    /* origin[4..7] = id0,id1,id2,id3 (LE on wire) */
    uint8_t o7 = id3;
    uint8_t o6 = id2;
    uint8_t o5 = id1;
    uint8_t o4 = id0;

    /* Decimal value as wmbusmeters computes it: (o7 & 0x03)<<24 | o6<<16
     * | o5<<8 | o4. Rendered to 8 digits with leading zeros. */
    uint32_t v = ((uint32_t)(o7 & 0x03) << 24) |
                 ((uint32_t)o6 << 16) |
                 ((uint32_t)o5 << 8)  |
                 (uint32_t)o4;
    char digits[16];
    snprintf(digits, sizeof(digits), "%08lu", (unsigned long)v);

    int yy = (digits[0] - '0') * 10 + (digits[1] - '0');
    int year = (yy > 70) ? (1900 + yy) : (2000 + yy);
    char serial[8] = {0};
    memcpy(serial, digits + 2, 6);

    char supplier = '@' + (uint8_t)(((c->medium & 0x0F) << 1) | (c->version >> 7));
    char meter_t  = '@' + (uint8_t)((c->version & 0x7C) >> 2);
    char diameter = '@' + (uint8_t)(((c->version & 0x03) << 3) | (o7 >> 5));

    char prefix[12];
    snprintf(prefix, sizeof(prefix), "%c%02d%c%c", supplier, yy, meter_t, diameter);

    pos = row(o, cap, pos, "Prefix",  "%s", prefix);
    pos = row(o, cap, pos, "Serial",  "%s", serial);
    pos = row(o, cap, pos, "Year",    "%d", year);
    return pos;
}

/* Build the alarm summary string from the three header bytes
 * (apdu[0..2] in our addressing). */
static void format_alarms(uint8_t a0, uint8_t a1, uint8_t a2,
                          char* cur, size_t cur_cap,
                          char* prv, size_t prv_cap) {
    bool general = (a0 >> 7) & 1;
    bool leak_now = (a1 >> 7) & 1;
    bool leak_prev = (a1 >> 6) & 1;
    bool blocked   = (a1 >> 5) & 1;
    bool back_flow = (a2 >> 7) & 1;
    bool underflow = (a2 >> 6) & 1;
    bool overflow  = (a2 >> 5) & 1;
    bool submarine = (a2 >> 4) & 1;
    bool sf_now    = (a2 >> 3) & 1;
    bool sf_prev   = (a2 >> 2) & 1;
    bool mf_now    = (a2 >> 1) & 1;
    bool mf_prev   = (a2 >> 0) & 1;

    /* Current alarms — short, comma-joined; 19 chars max for the value
     * column means we have to abbreviate aggressively. */
    if(general) {
        snprintf(cur, cur_cap, "general");
    } else {
        size_t p = 0;
        cur[0] = 0;
#define ADD(flag, txt) if((flag) && p + sizeof(txt) < cur_cap) { \
            if(p) cur[p++] = ','; \
            memcpy(cur + p, txt, sizeof(txt) - 1); p += sizeof(txt) - 1; \
            cur[p] = 0; \
        }
        ADD(leak_now, "leak");
        ADD(blocked,  "block");
        ADD(back_flow,"back");
        ADD(underflow,"undr");
        ADD(overflow, "over");
        ADD(submarine,"sub");
        ADD(sf_now,   "sfraud");
        ADD(mf_now,   "mfraud");
#undef ADD
        if(p == 0) snprintf(cur, cur_cap, "OK");
    }

    {
        size_t p = 0;
        prv[0] = 0;
#define ADDP(flag, txt) if((flag) && p + sizeof(txt) < prv_cap) { \
            if(p) prv[p++] = ','; \
            memcpy(prv + p, txt, sizeof(txt) - 1); p += sizeof(txt) - 1; \
            prv[p] = 0; \
        }
        ADDP(leak_prev, "leak");
        ADDP(sf_prev,   "sfraud");
        ADDP(mf_prev,   "mfraud");
#undef ADDP
        if(p == 0) snprintf(prv, prv_cap, "OK");
    }
}

static size_t decode_izar_ex(const WmbusDecodeCtx* ctx,
                             char* out, size_t cap) {
    if(!ctx || !out || cap == 0) return 0;
    if(!ctx->apdu || ctx->apdu_len < 5) {
        return (size_t)snprintf(out, cap,
            "Diehl IZAR water\nFrame too short\nBytes %u\n",
            (unsigned)ctx->apdu_len);
    }

    /* Try every default key in turn — most field meters speak with the
     * publicly-known seeds, so this Just Works without configuration. */
    uint8_t dec[256];
    size_t enc_len = ctx->apdu_len - 4;
    if(enc_len > sizeof(dec)) enc_len = sizeof(dec);

    bool ok = false;
    for(size_t k = 0; k < sizeof(k_prios_default_keys)/sizeof(uint32_t); k++) {
        uint32_t seed = prios_seed(k_prios_default_keys[k], ctx);
        if(prios_decrypt(seed, ctx->apdu + 4, enc_len, dec)) {
            ok = true; break;
        }
    }

    size_t pos = (size_t)snprintf(out, cap, "Diehl IZAR water\n");
    if(!ok) {
        pos += (size_t)snprintf(out + pos, cap - pos,
            "PRIOS key unknown\n"
            "Add custom key or\n"
            "report telegram on\n"
            "github.com/i12bp8/wmbuster\n"
            "Bytes %u\n", (unsigned)ctx->apdu_len);
        return pos;
    }

    /* Header bytes (battery, period, alarms) live in the *link-frame*
     * portion at apdu[0..2] (= frame[11..13] in wmbusmeters terms). */
    uint8_t h0 = ctx->apdu[0];
    uint8_t h1 = ctx->apdu[1];
    uint8_t h2 = ctx->apdu[2];
    double  battery_y = (h1 & 0x1F) / 2.0;
    int     period_s  = 1 << ((h0 & 0x0F) + 2);

    /* Total / last-month consumption in liters → m³ for display. */
    if(enc_len >= 5) {
        uint32_t total_l = u32le_at(dec + 1);
        pos = row(out, cap, pos, "Total", "%lu.%03lu m3",
                  (unsigned long)(total_l / 1000),
                  (unsigned long)(total_l % 1000));
    }
    if(enc_len >= 9) {
        uint32_t prev_l = u32le_at(dec + 5);
        pos = row(out, cap, pos, "Prev mon", "%lu.%03lu m3",
                  (unsigned long)(prev_l / 1000),
                  (unsigned long)(prev_l % 1000));
    }
    if(enc_len >= 11) {
        /* Date packed across dec[9..10]: year split bits, month low,
         * day low. The format is wmbusmeters-compatible. */
        uint16_t y = ((dec[10] & 0xF0) >> 1) + ((dec[9] & 0xE0) >> 5);
        y += (y > 80) ? 1900 : 2000;
        uint8_t m = dec[10] & 0x0F;
        uint8_t d = dec[9]  & 0x1F;
        pos = row(out, cap, pos, "Date", "%04u-%02u-%02u",
                  (unsigned)y, (unsigned)(m % 13), (unsigned)(d % 32));
    }

    pos = row(out, cap, pos, "Battery", "%u.%u y",
              (unsigned)(battery_y), (unsigned)((unsigned)(battery_y * 10) % 10));
    pos = row(out, cap, pos, "Period",  "%d s", period_s);

    if(ctx->manuf == MANUF_SAP) {
        pos = render_sap_prefix(out, cap, pos, ctx);
    }

    {
        char cur[24], prv[24];
        format_alarms(h0, h1, h2, cur, sizeof(cur), prv, sizeof(prv));
        pos = row(out, cap, pos, "Alarm now", "%s", cur);
        pos = row(out, cap, pos, "Alarm prv", "%s", prv);
    }
    return pos;
}

/* MVT triples taken directly from `_refs/wmbusmeters/src/driver_izar.cc`.
 * Note: (HYD,0x07,0x85) and (HYD,0x07,0x86) are claimed by both the IZAR
 * and the generic Hydrus driver in upstream — putting IZAR earlier in
 * the registry gives it priority, since PRIOS decoding produces real
 * fields where the OMS walker would only see noise. */
static const WmbusMVT k_mvt_izar[] = {
    {"HYD", 0x07, 0x85},
    {"HYD", 0x07, 0x86},
    {"DME", 0x06, 0x78},
    {"DME", 0x07, 0x78},
    {"SAP", 0x15, 0xFF},
    {"SAP", 0x04, 0xFF},
    {"SAP", 0x07, 0x00},
    { {0}, 0, 0 }
};

const WmbusDriver wmbus_drv_diehl_izar = {
    .id = "diehl-izar",
    .title = "Diehl IZAR water",
    .mvt = k_mvt_izar,
    .decode = NULL,
    .decode_ex = decode_izar_ex,
};
