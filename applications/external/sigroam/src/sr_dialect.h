#pragma once

#include "sr_types.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * ★ Pure-logic dialect switch. Must not include any furi header (ADR-003).
 *
 * Marauder text Mode A is the baseline; SigRoam firmware is an enhanced
 * peer that still speaks that dialect plus Radio:/Qual:/Sess:/Busy:/Diag:.
 * Session confirmation extras for stock Marauder (CSV-as-start, idle WiFi
 * stop) must not run on a SigRoam Version string, and must not run before
 * Probe/info has filled firmware.version (empty version keeps the historical
 * strict rules so existing host_test stays green).
 *
 * Discriminator is the Version: value substring "-sigroam-", copied from
 * firmware kVersion "v1.14.1-sigroam-<n>". Handshake Firmware: Marauder is
 * shared and must not be used as the switch.
 */

#define SR_DIALECT_SIGROAM_MARK "-sigroam-"

static inline bool sr_dialect_version_has_sigroam(const char* ver) {
    static const char k[] = SR_DIALECT_SIGROAM_MARK;
    size_t i;
    size_t j;

    if(ver == NULL) {
        return false;
    }
    for(i = 0; ver[i] != '\0'; i++) {
        for(j = 0; k[j] != '\0'; j++) {
            if(ver[i + j] != k[j]) {
                break;
            }
        }
        if(k[j] == '\0') {
            return true;
        }
    }
    return false;
}

static inline bool sr_dialect_is_sigroam(const SrFirmwareInfo* fw) {
    if(fw == NULL) {
        return false;
    }
    return sr_dialect_version_has_sigroam(fw->version);
}

/* Non-empty Version that is not a SigRoam enhanced build. */
static inline bool sr_dialect_is_generic_marauder(const SrFirmwareInfo* fw) {
    if(fw == NULL) {
        return false;
    }
    if(fw->version[0] == '\0') {
        return false;
    }
    return !sr_dialect_version_has_sigroam(fw->version);
}

/*
 * Dash may send `info` only to SigRoam firmware. Stock Marauder info is not
 * read-only: CommandLine INFO_CMD assigns currentScanMode = SHOW_INFO (101),
 * scanning() is then true (WIFI_SCAN_OFF is 0), and wardrive lives inside
 * if (!scanning()). Empty Version (pre-Probe) also skips, so a first Dash
 * open cannot swallow wardrive. Probe still sends info.
 */
static inline bool sr_dialect_dash_may_send_info(const SrFirmwareInfo* fw) {
    return sr_dialect_is_sigroam(fw);
}

/*
 * Stock Marauder INFO_CMD leaves currentScanMode=SHOW_INFO, so the next
 * wardrive is swallowed. Generic path only: Probe queues one stopscan after
 * handshake Ok; Dash queues one more only if Probe did not. SigRoam info is
 * read-only — never send this pair there (it would stop a live scan).
 *
 * Empty Version is unknown peer, not generic: SHOW_INFO-clear must wait until
 * Version is a non-sigroam string. ident_info_sent is kept so call sites stay
 * stable; it must not promote an empty Version into SendStop (xu182: that
 * made the first Dash OK a stopscan on SigRoam when info was late).
 *
 * Worker is a single slot: Probe and Dash must not both have stopscan in
 * flight. Confirm on wifi_stop_rev moving (Stopping WiFi always prints,
 * even when session is already Stopped), not on session_rev.
 */
typedef enum {
    SrShowInfoClearNone = 0, /* Not generic: send wardrive now */
    SrShowInfoClearSendStop, /* Generic, Probe did not queue: Dash sends stopscan */
    SrShowInfoClearWait /* Generic, Probe already queued: wait for wifi_stop_rev */
} SrShowInfoClearAct;

static inline bool sr_dialect_probe_should_clear_show_info(const SrFirmwareInfo* fw) {
    return sr_dialect_is_generic_marauder(fw);
}

static inline bool
    sr_dialect_needs_show_info_clear(const SrFirmwareInfo* fw, bool ident_info_sent) {
    (void)ident_info_sent;
    return sr_dialect_is_generic_marauder(fw);
}

static inline SrShowInfoClearAct sr_dialect_show_info_clear_on_start_ex(
    const SrFirmwareInfo* fw,
    bool probe_stop_sent,
    bool ident_info_sent) {
    if(!sr_dialect_needs_show_info_clear(fw, ident_info_sent)) {
        return SrShowInfoClearNone;
    }
    return probe_stop_sent ? SrShowInfoClearWait : SrShowInfoClearSendStop;
}

static inline SrShowInfoClearAct
    sr_dialect_show_info_clear_on_start(const SrFirmwareInfo* fw, bool probe_stop_sent) {
    return sr_dialect_show_info_clear_on_start_ex(fw, probe_stop_sent, false);
}

/* Wrapping counter: != , never >. */
static inline bool
    sr_dialect_show_info_clear_done(uint32_t wifi_stop_rev_now, uint32_t wifi_stop_rev_at_send) {
    return wifi_stop_rev_now != wifi_stop_rev_at_send;
}

/* sigroam_cap values. The byte is the pad in SrFirmwareInfo. */
enum {
    SR_SIGROAM_CAP_NONE = 0,
    SR_SIGROAM_CAP_ARMED = 1, /* saw #sigroam, waiting for Diag: or Sess: */
    SR_SIGROAM_CAP_YES = 2 /* peer answered the bare sigroam command */
};

/*
 * Last '#' on the line, then exactly `word` and nothing after.
 * `#sigroam` and `> #sigroam` both match "sigroam". `#info` does not.
 * Same echo shape as CommandLine::runCommand's Serial.println("#" + input).
 */
static inline bool
    sr_sigroam_echo_word(const char* line, size_t len, const char* word, size_t word_n) {
    size_t hash = len;
    size_t i;

    if(line == NULL || word == NULL) {
        return false;
    }
    for(i = 0; i < len; i++) {
        if(line[i] == '#') {
            hash = i;
        }
    }
    if(hash >= len || hash + 1u + word_n != len) {
        return false;
    }
    return memcmp(line + hash + 1u, word, word_n) == 0;
}

/*
 * Capability latch for a generic Marauder peer. A Diag: or Sess: line sets
 * YES only while ARMED by a `#sigroam` echo. The same lines after `#info`
 * (old SigRoam firmware) leave the latch alone. Once YES, it stays YES.
 */
static inline void sr_sigroam_cap_observe(uint8_t* cap, const char* line, size_t len) {
    size_t i;

    if(cap == NULL) {
        return;
    }
    if(*cap != (uint8_t)SR_SIGROAM_CAP_NONE && *cap != (uint8_t)SR_SIGROAM_CAP_ARMED &&
       *cap != (uint8_t)SR_SIGROAM_CAP_YES) {
        *cap = (uint8_t)SR_SIGROAM_CAP_NONE;
    }
    if(*cap == (uint8_t)SR_SIGROAM_CAP_YES) {
        return;
    }
    if(line == NULL) {
        len = 0u;
    }
    if(len >= 6u && memcmp(line, "Diag: ", 6) == 0) {
        if(*cap == (uint8_t)SR_SIGROAM_CAP_ARMED) {
            *cap = (uint8_t)SR_SIGROAM_CAP_YES;
        }
        return;
    }
    if(len >= 6u && memcmp(line, "Sess: ", 6) == 0) {
        if(*cap == (uint8_t)SR_SIGROAM_CAP_ARMED) {
            *cap = (uint8_t)SR_SIGROAM_CAP_YES;
        }
        return;
    }
    if(sr_sigroam_echo_word(line, len, "sigroam", 7u)) {
        *cap = (uint8_t)SR_SIGROAM_CAP_ARMED;
        return;
    }
    if(*cap == (uint8_t)SR_SIGROAM_CAP_ARMED && line != NULL) {
        for(i = 0; i < len; i++) {
            if(line[i] == '#') {
                *cap = (uint8_t)SR_SIGROAM_CAP_NONE;
                return;
            }
        }
    }
}

/*
 * Periodic and post-stop refresh. -sigroam- firmware keeps `info\n`.
 * A generic peer that answered `sigroam` gets `sigroam\n` and never `info`.
 * sr_dialect_dash_may_send_info stays the old predicate.
 */
static inline const char* sr_dialect_dash_refresh_cmd(const SrFirmwareInfo* fw) {
    if(sr_dialect_is_sigroam(fw)) {
        return "info\n";
    }
    if(fw != NULL && fw->sigroam_cap == (uint8_t)SR_SIGROAM_CAP_YES &&
       sr_dialect_is_generic_marauder(fw)) {
        return "sigroam\n";
    }
    return NULL;
}

/* One probe on a generic peer that has not answered yet. */
static inline bool sr_dialect_should_probe_sigroam(const SrFirmwareInfo* fw) {
    if(!sr_dialect_is_generic_marauder(fw)) {
        return false;
    }
    return fw->sigroam_cap != (uint8_t)SR_SIGROAM_CAP_YES;
}

/* Scout Lite / C5: answered sigroam, and Version has no "-sigroam-". */
static inline bool sr_dialect_lite_status(const SrFirmwareInfo* fw) {
    return fw != NULL && fw->sigroam_cap == (uint8_t)SR_SIGROAM_CAP_YES &&
           !sr_dialect_is_sigroam(fw);
}

/*
 * Strict "Busy: st=<u> seal=<hex>" on a lite peer is the second line of
 * printSigRoamStatus, not a command refusal. Stock Marauder does not emit
 * that shape ("Busy: stopscan first" fails the parser). Old -sigroam-
 * firmware still refuses with it.
 */
static inline bool sr_dialect_busy_is_refusal(const SrFirmwareInfo* fw) {
    return !sr_dialect_lite_status(fw);
}

/*
 * 1 online, 0 offline, -1 unknown.
 * Non-lite identity: Qual.gga > 0 means sentences have arrived since boot.
 * A lite peer uses sr_lite_gps_online (the counter moved since the previous
 * Qual sample). Without Qual, Diag hb[0] is the gps slot; this firmware
 * writes sats there. hb[1] is a hardcoded 1 in that print and is not an
 * online bit. SD: Qual.sd, else Diag hb[4] (store slot, sd_up).
 */
static inline int sr_ident_gps_online(bool diag_seen, uint32_t hb0, bool qual_seen, uint32_t gga) {
    if(qual_seen) {
        return gga > 0u ? 1 : 0;
    }
    if(diag_seen) {
        return hb0 > 0u ? 1 : 0;
    }
    return -1;
}

static inline int sr_ident_sd_online(bool diag_seen, uint32_t hb4, bool qual_seen, uint8_t sd) {
    if(qual_seen) {
        return sd != 0u ? 1 : 0;
    }
    if(diag_seen) {
        return hb4 != 0u ? 1 : 0;
    }
    return -1;
}

static inline const char* sr_ident_onoff(int v) {
    if(v > 0) {
        return "online";
    }
    if(v == 0) {
        return "offline";
    }
    return "?";
}

static inline size_t sr_ident_fmt(
    const SrFirmwareInfo* fw,
    bool qual_seen,
    const SrQualInfo* qual,
    const char* brand,
    const char* product,
    char* out,
    size_t cap) {
    const char* name;
    const char* ver;
    uint32_t gga = 0u;
    uint8_t sd = 0u;
    int n;

    if(out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if(fw == NULL) {
        return 0u;
    }
    if(qual_seen && qual != NULL) {
        gga = qual->gga;
        sd = qual->sd;
    } else {
        qual_seen = false;
    }
    name = fw->firmware[0] != '\0' ? fw->firmware : "?";
    ver = fw->version[0] != '\0' ? fw->version : "?";
    if(brand == NULL || brand[0] == '\0') {
        brand = "?";
    }
    if(product == NULL || product[0] == '\0') {
        product = "?";
    }
    n = snprintf(
        out,
        cap,
        "%s\n%s\nFirmware: %s\nVersion: %s\nGPS %s\nSD %s",
        brand,
        product,
        name,
        ver,
        sr_ident_onoff(sr_ident_gps_online(fw->diag_seen, fw->diag_hb[0], qual_seen, gga)),
        sr_ident_onoff(sr_ident_sd_online(fw->diag_seen, fw->diag_hb[4], qual_seen, sd)));
    if(n < 0) {
        out[0] = '\0';
        return 0u;
    }
    if((size_t)n >= cap) {
        return cap - 1u;
    }
    return (size_t)n;
}

/*
 * 1 = counter moved, 0 = stalled, -1 = no previous sample.
 * (now - prev) != 0 wraps. now > prev treats UINT32_MAX -> 0 as stalled.
 */
static inline int sr_lite_counter_live(bool have_prev, uint32_t prev, uint32_t now) {
    if(!have_prev) {
        return -1;
    }
    return (now - prev) != 0u ? 1 : 0;
}

/*
 * Lite GPS online. A Qual sample uses the gga delta, including the first
 * sample (no previous -> unknown). Diag hb[0] applies only when no Qual
 * has arrived at all.
 */
static inline int sr_lite_gps_online(
    bool qual_seen,
    bool have_prev,
    uint32_t prev_gga,
    uint32_t gga,
    bool diag_seen,
    uint32_t hb0) {
    if(qual_seen) {
        return sr_lite_counter_live(have_prev, prev_gga, gga);
    }
    if(diag_seen) {
        return hb0 > 0u ? 1 : 0;
    }
    return -1;
}

/* Same 15 s window as SR_QUAL_STALE_MS. Copied so this header does not
 * include the view formatter. */
enum {
    SR_IDENT_QUAL_STALE_MS = 15000
};

/* Widget \e# is FontPrimary, Flipper's u8g2_font_helvB08_tr. This is the
 * glyph advance (delta-x) for ASCII 32..126, summed. The sum is about 1 px
 * over the drawn width. Measured 2026-10-20 from the upstream font file.
 * "PINGEQUA Scout Lite" is 108 and fits in 128. Eight W plus " Scout Lite"
 * is 141 and does not. */
static inline uint16_t sr_ident_primary_advance(const char* s) {
    static const uint8_t k_dx[95] = {3, 4, 5, 6, 6, 8, 8, 3,  4, 4, 4, 6, 3, 5,  3, 4, 6, 6,  6,
                                     6, 6, 6, 6, 6, 6, 6, 3,  3, 5, 6, 5, 6, 11, 8, 7, 8, 7,  6,
                                     6, 8, 7, 3, 6, 7, 6, 10, 8, 8, 7, 8, 7, 7,  7, 7, 8, 11, 8,
                                     9, 7, 4, 4, 4, 5, 6, 3,  6, 6, 5, 6, 6, 4,  6, 6, 3, 3,  6,
                                     3, 9, 6, 6, 6, 6, 4, 6,  4, 6, 6, 8, 7, 6,  6, 5, 3, 5,  6};
    uint16_t w = 0u;

    if(s == NULL) {
        return 0u;
    }
    for(; *s != '\0'; s++) {
        unsigned char c = (unsigned char)*s;
        if(c < 32u || c > 126u) {
            w = (uint16_t)(w + 8u);
        } else {
            w = (uint16_t)(w + k_dx[c - 32u]);
        }
    }
    return w;
}

/* Lite Probe Ok page. Bold title (\x1b#, widget \e#). One line when the
 * FontPrimary advance fits in 128 px, otherwise brand and product on two
 * bold lines. Description is the locked copy. The version line is
 * Firmware + Version with no labels. GPS/SD share one line.
 *
 * fix/nofix is sr_lite_counter_live on ggafix, the same rule as the Dash
 * row. The raw counter is never copied. No previous sample, no Qual, or
 * a sample older than 15 s is "GPS:?".
 *
 * SD is the boot-time card detect. Firmware sd_obj.supported is sampled
 * once at startup; pulling the card later still reads as present
 * (2026-10-20). This line shows that boot value. A live re-check belongs
 * on the firmware. sd == 0 replaces the whole line with "SD:no". */
static inline size_t sr_ident_fmt_lite(
    const SrFirmwareInfo* fw,
    bool qual_seen,
    const SrQualInfo* qual,
    bool qual_prev_seen,
    uint32_t prev_ggafix,
    uint32_t qual_tick_ms,
    uint32_t now_ms,
    const char* brand,
    const char* product,
    char* out,
    size_t cap) {
    char title[64];
    char gps[40];
    const char* name;
    const char* ver;
    const char* sd_tag;
    uint32_t ggafix = 0u;
    uint8_t sd = 0u;
    uint8_t sats = 0u;
    int face;
    int nt;
    int n;
    int sd_on;
    bool fresh;
    bool one_line;

    if(out == NULL || cap == 0u) {
        return 0u;
    }
    out[0] = '\0';
    if(fw == NULL) {
        return 0u;
    }
    if(qual_seen && qual != NULL) {
        ggafix = qual->ggafix;
        sd = qual->sd;
        sats = qual->sats;
    } else {
        qual_seen = false;
    }
    fresh = qual_seen && (now_ms - qual_tick_ms) <= (uint32_t)SR_IDENT_QUAL_STALE_MS;
    name = fw->firmware[0] != '\0' ? fw->firmware : "?";
    ver = fw->version[0] != '\0' ? fw->version : "?";
    if(brand == NULL || brand[0] == '\0') {
        brand = "?";
    }
    if(product == NULL || product[0] == '\0') {
        product = "?";
    }

    sd_on = sr_ident_sd_online(fw->diag_seen, fw->diag_hb[4], qual_seen, sd);
    if(sd_on == 0) {
        gps[0] = 'S';
        gps[1] = 'D';
        gps[2] = ':';
        gps[3] = 'n';
        gps[4] = 'o';
        gps[5] = '\0';
    } else {
        if(!fresh) {
            face = -1;
        } else {
            face = sr_lite_counter_live(qual_prev_seen, prev_ggafix, ggafix);
        }
        if(face > 0) {
            (void)snprintf(gps, sizeof(gps), "GPS:%usat fix", (unsigned)sats);
        } else if(face == 0) {
            (void)snprintf(gps, sizeof(gps), "GPS:%usat nofix", (unsigned)sats);
        } else {
            gps[0] = 'G';
            gps[1] = 'P';
            gps[2] = 'S';
            gps[3] = ':';
            gps[4] = '?';
            gps[5] = '\0';
        }
        sd_tag = sd_on > 0 ? "  SD:ok" : "  SD:?";
        {
            size_t used = 0u;
            while(gps[used] != '\0' && used + 1u < sizeof(gps)) {
                used++;
            }
            if(used + 7u < sizeof(gps)) {
                size_t i;
                for(i = 0u; sd_tag[i] != '\0'; i++) {
                    gps[used + i] = sd_tag[i];
                }
                gps[used + i] = '\0';
            }
        }
    }

    nt = snprintf(title, sizeof(title), "%s %s", brand, product);
    one_line = nt > 0 && (size_t)nt < sizeof(title) && sr_ident_primary_advance(title) <= 128u;
    if(one_line) {
        n = snprintf(
            out,
            cap,
            "\x1b#%s\n"
            "Dual-band Wardriver\n"
            "%s %s\n"
            "%s",
            title,
            name,
            ver,
            gps);
    } else {
        n = snprintf(
            out,
            cap,
            "\x1b#%s\n"
            "\x1b#%s\n"
            "Dual-band Wardriver\n"
            "%s %s\n"
            "%s",
            brand,
            product,
            name,
            ver,
            gps);
    }
    if(n < 0) {
        out[0] = '\0';
        return 0u;
    }
    if((size_t)n >= cap) {
        return cap - 1u;
    }
    return (size_t)n;
}
