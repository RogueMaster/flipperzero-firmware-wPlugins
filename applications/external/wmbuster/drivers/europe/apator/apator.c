/* Apator (Poland) — diverse portfolio: amiplus elec, 162/172
 * water, 08 gas, ELF heat, OP-041A water, Ultrimis water,
 * Powogaz water (APT), Aquastream (IMT).
 *
 * wmbusmeters refs: drivers/src/{amiplus,apator08,apator162,
 *   apator172,aptmbusna,op041a,elf,elf2,ultrimis,aquastream}.xmq.
 */

#include "../../engine/driver.h"

#include <stdio.h>

/* amiplus — smart electricity (relabelled by NES, DEV, APT). */
static const WmbusMVT k_mvt_amiplus[] = {
    {"APA", 0x01, 0x02},
    {"APA", 0x02, 0x02},
    {"APT", 0x01, 0x02},
    {"NES", 0x03, 0x02},
    {"DEV", 0x00, 0x02},
    {"DEV", 0x01, 0x02},
    {"DEV", 0x02, 0x37},
    {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_amiplus = {
    .id = "amiplus",
    .title = "Apator amiplus elec",
    .mvt = k_mvt_amiplus,
    .decode = NULL,
};

/* Apator 162 water */
static const WmbusMVT k_mvt_a162[] = {{"APA", 0x05, 0x06}, {"APA", 0x05, 0x07}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_162 = {
    .id = "apator162",
    .title = "Apator 162 water",
    .mvt = k_mvt_a162,
    .decode = NULL,
};

/* Apator 172 water */
static const WmbusMVT k_mvt_a172[] = {{"APT", 0x04, 0x11}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_172 = {
    .id = "apator172",
    .title = "Apator 172 water",
    .mvt = k_mvt_a172,
    .decode = NULL,
};

/* Apator 08 gas */
static const WmbusMVT k_mvt_a08[] = {{"APT", 0x03, 0x03}, {"APT", 0x0F, 0x0F}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_08 = {
    .id = "apator08",
    .title = "Apator 08 gas",
    .mvt = k_mvt_a08,
    .decode = NULL,
};

/* MBUS-NA1 / NA-1 water — the (APA, 0x07, 0x14) tuple now lives in
 * `apator_na1.c` which carries the real "needs key" decoder. The
 * (APA, 0x15, 0x07) tuple stays here as an OMS-walker fallback for the
 * non-encrypted variant of the meter. */
static const WmbusMVT k_mvt_na1_oms[] = {{"APA", 0x15, 0x07}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_na1_oms = {
    .id = "apator-na1-oms",
    .title = "Apator NA-1 water",
    .mvt = k_mvt_na1_oms,
    .decode = NULL,
};

/* OP-041A water */
static const WmbusMVT k_mvt_op041a[] = {{"APA", 0x1A, 0x07}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_op041a = {
    .id = "apator-op041a",
    .title = "Apator OP-041A water",
    .mvt = k_mvt_op041a,
    .decode = NULL,
};

/* ELF / ELF2 heat */
static const WmbusMVT k_mvt_elf[] =
    {{"APA", 0x40, 0x04}, {"APA", 0x42, 0x04}, {"APA", 0x42, 0x0D}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_elf = {
    .id = "apator-elf",
    .title = "Apator ELF heat",
    .mvt = k_mvt_elf,
    .decode = NULL,
};

/* Apator HCA / E.ITN (heat-cost allocator) — 1:1 port of the
 * wmbusmeters `apatoreitn` driver (drivers/src/apatoreitn.xmq).
 *
 * CI=0xA0 mfct-specific payload, 15 bytes:
 *   b0     season_start_date_lo  (raw date = 0xA000 | lo)
 *   b1-2   pad
 *   b3-4   previous_hca      uint16 LE (storagenr 1)
 *   b5-6   esb_date_raw      uint16 LE, 0 = seal intact
 *   b7-8   current_hca       uint16 LE (storagenr 0)
 *   b9-10  current_date      packed date
 *   b11-12 temp_room_prev_avg  /256 °C (matches on-meter display)
 *   b13-14 temp_room_avg       /256 °C
 *
 * Packed date (all three date fields, year base 2000):
 *   day = raw % 32, month = (raw >> 5) % 16, year = 2000 + (raw >> 9) % 32
 *
 * Upstream accepts three wrappings of the same 15-byte frame:
 * bare, 0xA0-prefixed, and the CI=0xB6 form
 * <n> <n bytes> 0xA0 <15 bytes> with n = 0x00..0x0F. */
static uint16_t apator_hca_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void apator_hca_date(uint16_t raw, char* buf, size_t cap) {
    unsigned day = raw % 32u;
    unsigned month = (raw >> 5) % 16u;
    unsigned year = 2000u + ((raw >> 9) % 32u);
    snprintf(buf, cap, "%04u-%02u-%02u", year, month, day);
}

/* Locate the 15-byte direct frame inside the APDU, or NULL. */
static const uint8_t* apator_hca_frame(const uint8_t* a, size_t len) {
    if(len == 15) return a;
    if(len == 16 && a[0] == 0xA0) return a + 1;
    if(len >= 17 && a[0] <= 0x0F && len == (size_t)(2u + a[0] + 15u) && a[1 + a[0]] == 0xA0)
        return a + 2 + a[0];
    return NULL;
}

static size_t apator_hca_fallback(const uint8_t* a, size_t len, char* o, size_t cap) {
    size_t pos = (size_t)snprintf(o, cap, "Apator HCA\n");
    pos += (size_t)snprintf(
        o + pos,
        cap - pos,
        "Bytes %u\n"
        "Status bad length\n",
        (unsigned)len);

    size_t off = 0;
    int rows = 0;
    while(off < len && rows < 3 && pos + 24 < cap) {
        size_t n = len - off;
        if(n > 6) n = 6;
        pos += (size_t)snprintf(o + pos, cap - pos, "Hex%02u ", (unsigned)off);
        for(size_t i = 0; i < n && pos + 3 < cap; i++) {
            pos += (size_t)snprintf(o + pos, cap - pos, "%02X", a[off + i]);
        }
        if(pos + 1 < cap) {
            o[pos++] = '\n';
            o[pos] = '\0';
        }
        off += n;
        rows++;
    }

    return pos;
}

static size_t decode_apator_hca(
    uint16_t m,
    uint8_t v,
    uint8_t med,
    const uint8_t* a,
    size_t len,
    char* o,
    size_t cap) {
    (void)m;
    (void)v;
    (void)med;

    const uint8_t* p = apator_hca_frame(a, len);
    if(!p) return apator_hca_fallback(a, len, o, cap);

    const uint16_t previous_hca = apator_hca_le16(p + 3);
    const uint16_t esb_raw = apator_hca_le16(p + 5);
    const uint16_t current_hca = apator_hca_le16(p + 7);
    const uint16_t date_raw = apator_hca_le16(p + 9);
    const uint16_t temp_prev_raw = apator_hca_le16(p + 11);
    const uint16_t temp_avg_raw = apator_hca_le16(p + 13);
    const uint16_t season_raw = (uint16_t)0xA000 | p[0];

    char date[12], season[12], esb[12];
    apator_hca_date(date_raw, date, sizeof(date));
    apator_hca_date(season_raw, season, sizeof(season));
    if(esb_raw) apator_hca_date(esb_raw, esb, sizeof(esb));

    size_t pos = (size_t)snprintf(
        o,
        cap,
        "Apator HCA\n"
        "Current %u\n"
        "Previous %u\n"
        "Date %s\n"
        "Season %s\n"
        "Seal %s\n",
        current_hca,
        previous_hca,
        date,
        season,
        esb_raw ? "BROKEN" : "OK");
    if(esb_raw) pos += (size_t)snprintf(o + pos, cap - pos, "ESBDate %s\n", esb);
    pos += (size_t)snprintf(
        o + pos,
        cap - pos,
        "TempPrev %u.%03u C\n"
        "TempAvg %u.%03u C\n",
        (unsigned)(temp_prev_raw >> 8),
        (unsigned)((temp_prev_raw & 0xFFu) * 1000u + 128u) / 256u,
        (unsigned)(temp_avg_raw >> 8),
        (unsigned)((temp_avg_raw & 0xFFu) * 1000u + 128u) / 256u);
    return pos;
}

static const WmbusMVT k_mvt_apator_hca[] = {
    {"APA", 0x04, 0x08},
    {"APT", 0x04, 0x08},
    {"APA", 0x08, 0x04},
    {"APT", 0x08, 0x04},
    {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_hca = {
    .id = "apator-hca",
    .title = "Apator HCA",
    .mvt = k_mvt_apator_hca,
    .decode = decode_apator_hca,
};

/* Ultrimis water (cold) */
static const WmbusMVT k_mvt_ultrimis[] = {{"APA", 0x01, 0x16}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_ultrimis = {
    .id = "ultrimis",
    .title = "Apator Ultrimis water",
    .mvt = k_mvt_ultrimis,
    .decode = NULL,
};

/* Apator Powogaz / Aquastream (IMT) */
static const WmbusMVT k_mvt_aquastream[] = {{"IMT", 0x01, 0x07}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_apator_aquastream = {
    .id = "aquastream",
    .title = "Apator Aquastream",
    .mvt = k_mvt_aquastream,
    .decode = NULL,
};
