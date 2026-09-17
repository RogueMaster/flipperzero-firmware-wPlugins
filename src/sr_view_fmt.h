#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sr_capture_health.h"

/*
 * ★ Pure formatting for hand-drawn views. Must not include any furi header (ADR-003 / ADR-019 decision 4).
 * Numeric formatting, tab wrapping, truncation by character count. The draw callback only consumes results from here.
 */

enum {
    SR_VIEW_TAB_DASH = 0,
    SR_VIEW_TAB_STREAM,
    SR_VIEW_TAB_GPS,
    SR_VIEW_TAB_SESSION,
    SR_VIEW_TAB_COUNT
};

/* Safe upper bound for FontSecondary at 128 px; originally from scenes/scene_drive.c:38-45 */
enum { SR_VIEW_COLS = 20 };

static inline uint8_t sr_view_tab_next(uint8_t cur, int dir) {
    if(cur >= (uint8_t)SR_VIEW_TAB_COUNT) {
        return (uint8_t)SR_VIEW_TAB_DASH;
    }
    if(dir == 1) {
        return (uint8_t)((cur + 1u) % (uint8_t)SR_VIEW_TAB_COUNT);
    }
    if(dir == -1) {
        return (uint8_t)((cur + (uint8_t)SR_VIEW_TAB_COUNT - 1u) % (uint8_t)SR_VIEW_TAB_COUNT);
    }
    return cur;
}

/* Write a decimal into out. On insufficient cap, truncate to a prefix and still NUL-terminate. Returns the written length (excluding the NUL). */
static inline size_t sr_fmt__udec(uint32_t v, char* out, size_t cap) {
    char digs[10];
    size_t nd = 0;
    size_t i;
    uint32_t x;

    if(out == NULL || cap == 0) {
        return 0;
    }
    if(v == 0u) {
        if(cap < 2u) {
            out[0] = '\0';
            return 0;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }
    x = v;
    while(x > 0u && nd < sizeof(digs)) {
        digs[nd++] = (char)('0' + (x % 10u));
        x /= 10u;
    }
    if(nd + 1u > cap) {
        size_t w = cap - 1u;
        for(i = 0; i < w; i++) {
            out[i] = digs[nd - 1u - i];
        }
        out[w] = '\0';
        return w;
    }
    for(i = 0; i < nd; i++) {
        out[i] = digs[nd - 1u - i];
    }
    out[nd] = '\0';
    return nd;
}

static inline size_t sr_fmt__cpy(const char* src, size_t n, char* out, size_t cap) {
    size_t w;
    size_t i;

    if(out == NULL || cap == 0) {
        return 0;
    }
    w = n;
    if(w + 1u > cap) {
        w = cap - 1u;
    }
    for(i = 0; i < w; i++) {
        out[i] = src[i];
    }
    out[w] = '\0';
    return w;
}

static inline size_t sr_fmt_bytes(uint32_t bytes, char* out, size_t cap) {
    char tmp[16];
    size_t n;
    uint32_t unit;
    uint32_t whole;
    uint32_t tenth;
    char suffix;

    if(out == NULL || cap == 0) {
        return 0;
    }

    if(bytes < 1024u) {
        n = sr_fmt__udec(bytes, tmp, sizeof(tmp));
        if(n + 1u < sizeof(tmp)) {
            tmp[n++] = 'B';
            tmp[n] = '\0';
        }
        return sr_fmt__cpy(tmp, n, out, cap);
    }

    if(bytes < (1024u * 1024u)) {
        unit = 1024u;
        suffix = 'K';
    } else {
        unit = 1024u * 1024u;
        suffix = 'M';
    }
    whole = bytes / unit;
    tenth = (bytes % unit) * 10u / unit;
    n = sr_fmt__udec(whole, tmp, sizeof(tmp));
    if(n + 3u < sizeof(tmp)) {
        tmp[n++] = '.';
        tmp[n++] = (char)('0' + (char)tenth);
        tmp[n++] = suffix;
        tmp[n] = '\0';
    }
    return sr_fmt__cpy(tmp, n, out, cap);
}

static inline void sr_fmt__pad2(uint32_t v, char* p) {
    p[0] = (char)('0' + ((v / 10u) % 10u));
    p[1] = (char)('0' + (v % 10u));
}

static inline size_t sr_fmt_duration(uint32_t ms, char* out, size_t cap) {
    char tmp[16];
    size_t n;
    uint32_t total_s;
    uint32_t s;
    uint32_t total_m;
    uint32_t m;
    uint32_t h;

    if(out == NULL || cap == 0) {
        return 0;
    }

    total_s = ms / 1000u;
    s = total_s % 60u;
    total_m = total_s / 60u;
    m = total_m % 60u;
    h = total_m / 60u;

    if(ms < 3600000u) {
        /* MM:SS. Under one hour the minutes run 0..59, zero-padded to two digits. */
        sr_fmt__pad2(total_m, tmp);
        tmp[2] = ':';
        sr_fmt__pad2(s, tmp + 3);
        tmp[5] = '\0';
        n = 5;
    } else {
        n = sr_fmt__udec(h, tmp, sizeof(tmp));
        if(n + 6u < sizeof(tmp)) {
            tmp[n++] = ':';
            sr_fmt__pad2(m, tmp + n);
            n += 2u;
            tmp[n++] = ':';
            sr_fmt__pad2(s, tmp + n);
            n += 2u;
            tmp[n] = '\0';
        }
    }
    return sr_fmt__cpy(tmp, n, out, cap);
}

static inline uint8_t sr_fmt_rssi_bars(int8_t rssi) {
    if(rssi >= -55) {
        return 4;
    }
    if(rssi >= -70) {
        return 3;
    }
    if(rssi >= -80) {
        return 2;
    }
    if(rssi >= -90) {
        return 1;
    }
    return 0;
}

static inline char sr_fmt__san(unsigned char c) {
    if(c < 0x20u || c > 0x7Eu) {
        return '.';
    }
    return (char)c;
}

/*
 * Truncate by bytes to max_cols. Only [src, src+len) is guaranteed readable on src;
 * src[len] is not guaranteed to be a terminator (ADR-010). Do not scan it as a C string.
 * When it overflows, the last byte is written as '~'.
 */
static inline size_t sr_fmt_fit(const char* src, size_t len, size_t max_cols, char* out, size_t cap) {
    size_t n;
    size_t i;

    if(out == NULL || cap == 0) {
        return 0;
    }
    if(src == NULL) {
        out[0] = '\0';
        return 0;
    }

    n = len;
    if(n > max_cols) {
        n = max_cols;
    }
    if(n >= cap) {
        n = cap - 1u;
    }

    if(n > 0) {
        memcpy(out, src, n);
        for(i = 0; i < n; i++) {
            out[i] = sr_fmt__san((unsigned char)out[i]);
        }
    }
    if(len > max_cols && n > 0) {
        out[n - 1u] = '~';
    }
    out[n] = '\0';
    return n;
}

enum { SR_STREAM_ROWS = 5, SR_STREAM_COLS = 17 };

/* Return the index of the first NUL within the first cap bytes of s, or cap if there is none. Never reads s[cap]. */
static inline size_t sr_fmt_bounded_len(const char* s, size_t cap) {
    size_t i;

    if(s == NULL || cap == 0) {
        return 0;
    }
    for(i = 0; i < cap; i++) {
        if(s[i] == '\0') {
            return i;
        }
    }
    return cap;
}

static inline uint16_t sr_stream_clamp_top(uint16_t top, uint16_t count, uint8_t rows) {
    uint16_t max_top;

    if(count == 0 || rows == 0) {
        return 0;
    }
    if(count <= (uint16_t)rows) {
        return 0;
    }
    max_top = (uint16_t)(count - (uint16_t)rows);
    if(top > max_top) {
        return max_top;
    }
    return top;
}

/* dir=+1 moves older, dir=-1 moves newer. Saturates rather than wrapping. Any other dir only clamps. */
static inline uint16_t sr_stream_scroll(uint16_t top, uint16_t count, uint8_t rows, int dir) {
    if(dir == 1) {
        if(top < 0xFFFFu) {
            top = (uint16_t)(top + 1u);
        }
        return sr_stream_clamp_top(top, count, rows);
    }
    if(dir == -1) {
        if(top == 0) {
            return sr_stream_clamp_top(0, count, rows);
        }
        return sr_stream_clamp_top((uint16_t)(top - 1u), count, rows);
    }
    return sr_stream_clamp_top(top, count, rows);
}

/*
 * The text portion of a Stream row (excluding bars and the scrollbar).
 * Only [ssid, ssid+ssid_len) is guaranteed readable. Do not scan it as a C string.
 */
static inline size_t sr_fmt_stream_row(
    const char* ssid,
    size_t ssid_len,
    bool ble,
    uint8_t channel,
    size_t max_cols,
    char* out,
    size_t cap) {
    size_t pos;
    size_t cols;
    size_t ch_need;
    size_t ch_nd;
    char chbuf[4];
    size_t ssid_budget;
    const char* src;
    size_t src_len;
    size_t wrote;
    size_t i;

    if(out == NULL || cap == 0) {
        return 0;
    }

    pos = 0;
    cols = 0;
    ch_need = 0;
    ch_nd = 0;

    if(channel != 0) {
        ch_nd = sr_fmt__udec((uint32_t)channel, chbuf, sizeof(chbuf));
        ch_need = 1u + ch_nd;
    }

    if(ssid == NULL || ssid_len == 0) {
        src = "(hidden)";
        src_len = 8;
    } else {
        src = ssid;
        src_len = ssid_len;
    }

    /* Deduct the BLE prefix first, then the channel segment; whatever remains goes to the SSID. */
    ssid_budget = max_cols;
    if(ble && ssid_budget > 0) {
        ssid_budget--;
    }
    if(ssid_budget > ch_need) {
        ssid_budget -= ch_need;
    } else {
        ssid_budget = 0;
    }

    if(ble && cols < max_cols && pos + 1u < cap) {
        out[pos++] = '*';
        cols++;
    }

    if(ssid_budget > 0 && pos + 1u < cap) {
        wrote = sr_fmt_fit(src, src_len, ssid_budget, out + pos, cap - pos);
        pos += wrote;
        cols += wrote;
    }

    if(channel != 0) {
        if(cols < max_cols && pos + 1u < cap) {
            out[pos++] = ' ';
            cols++;
        }
        for(i = 0; i < ch_nd; i++) {
            if(cols < max_cols && pos + 1u < cap) {
                out[pos++] = chbuf[i];
                cols++;
            }
        }
    }

    if(pos < cap) {
        out[pos] = '\0';
    } else {
        out[cap - 1u] = '\0';
        pos = cap - 1u;
    }
    return pos;
}

/*
 * GPS numeric field -> display string.
 *
 * fix == false, or an empty value -> write "--".
 * **Never display the raw value directly**: without a fix Marauder prints "0.0000000"
 * (not an empty string), which on screen is a Null Island fake coordinate that looks like real data.
 * Evidence: tools/host_test/fixtures/gpsdata.bin (captured from real hardware in T0.3).
 *
 * Only [val, val + bounded_len) is guaranteed readable on val; handle it per ADR-010 and never scan it to a terminator.
 * Returns the written length (excluding the NUL).
 */
static inline size_t sr_fmt_gps_val(
    const char* val, size_t val_cap, bool fix, size_t max_cols, char* out, size_t out_cap) {
    size_t vlen;

    if(out == NULL || out_cap == 0) {
        return 0;
    }
    if(!fix) {
        return sr_fmt_fit("--", 2, max_cols, out, out_cap);
    }
    if(val == NULL) {
        return sr_fmt_fit("--", 2, max_cols, out, out_cap);
    }
    vlen = sr_fmt_bounded_len(val, val_cap);
    if(vlen == 0) {
        return sr_fmt_fit("--", 2, max_cols, out, out_cap);
    }
    return sr_fmt_fit(val, vlen, max_cols, out, out_cap);
}

/*
 * Fifth row of the GPS tab:
 *   With a fix and a non-empty datetime -> the device timestamp verbatim (SR_DATETIME_MAX=19, which is just within SR_VIEW_COLS=20)
 *   Otherwise               -> "Blocks: <n>" (without a fix this proves the gpsdata link is still receiving, which beats a row of "--")
 * blocks goes through sr_fmt__udec; overflow is likewise caught by the '~' in sr_fmt_fit.
 */
static inline size_t sr_fmt_gps_stamp(
    const char* dt, size_t dt_cap, bool fix, uint32_t blocks, char* out, size_t cap) {
    char tmp[32];
    size_t n;
    size_t dlen;

    if(out == NULL || cap == 0) {
        return 0;
    }
    if(fix) {
        dlen = sr_fmt_bounded_len(dt, dt_cap);
        if(dlen > 0) {
            return sr_fmt_fit(dt, dlen, (size_t)SR_VIEW_COLS, out, cap);
        }
    }
    n = sr_fmt__cpy("Blocks: ", 8, tmp, sizeof(tmp));
    n += sr_fmt__udec(blocks, tmp + n, sizeof(tmp) - n);
    return sr_fmt_fit(tmp, n, (size_t)SR_VIEW_COLS, out, cap);
}

/*
 * SrSessionState numeric value -> display label.
 *
 * The ★ layer cannot see SrSessionState (sr_view_fmt.h includes only stdbool/stddef/stdint/string.h,
 * and the selfcontained target compiles it in isolation), so it maps by numeric value:
 *   0 = Idle / 1 = Running / 2 = Stopped (read from src/sr_model.h:37-41)
 * The numeric assumption is pinned at compile time by the _Static_assert in views/sr_view_dash.c.
 * Out of range always returns "?", never NULL (the caller feeds it straight to canvas_draw_str).
 */
static inline const char* sr_fmt_session_label(uint8_t state) {
    if(state == 0) {
        return "Idle";
    }
    if(state == 1) {
        return "Running";
    }
    if(state == 2) {
        return "Stopped";
    }
    return "?";
}

/*
 * Hardware: prefix names the Scout Lite board, not the firmware.
 * Firmware/app are SigRoam. Wire Firmware: stays "Marauder" (handshake).
 * Bounded: never scan past hw_cap. Case-sensitive, same prefix as Probe.
 */
static inline bool sr_fmt_hw_is_scout_lite(const char* hw, size_t hw_cap) {
    static const char k[] = "Scout Lite";
    const size_t klen = sizeof(k) - 1u;
    size_t n;
    size_t i;

    n = sr_fmt_bounded_len(hw, hw_cap);
    if(n < klen) {
        return false;
    }
    for(i = 0; i < klen; i++) {
        if(hw[i] != k[i]) {
            return false;
        }
    }
    return true;
}

/*
 * Sess tab status line for Scout Lite. Never copies the wire Firmware:
 * token. Fitted to SR_VIEW_COLS.
 *
 * state: Diag: SCANNING(1)→Running, SEALED(4)→Sealed; else FAP session
 * label (Idle/Running/Stopped). duration: live elapsed while FAP session
 * is Running (numeric 1), else board sess_ms. radio: permission bits;
 * omitted when radio_rev==0. "Running 01:26 WiFi BLE" is 21 cols, so the
 * radio token is W+B / W / B / --.
 */
static inline size_t sr_fmt_sess_sigroam_status(
    uint8_t session,
    bool diag_seen,
    uint8_t diag_state,
    uint32_t elapsed_ms,
    uint32_t sess_ms,
    uint32_t radio_rev,
    uint8_t radio_wifi,
    uint8_t radio_ble,
    char* out,
    size_t cap) {
    char tmp[40];
    char dur[16];
    const char* st;
    size_t n;
    size_t slen;
    uint32_t ms;

    if(out == NULL || cap == 0) {
        return 0;
    }

    if(diag_seen && diag_state == 4u) {
        st = "Sealed";
    } else if(diag_seen && diag_state == 1u) {
        st = "Running";
    } else {
        st = sr_fmt_session_label(session);
    }

    ms = (session == 1u) ? elapsed_ms : sess_ms;
    (void)sr_fmt_duration(ms, dur, sizeof(dur));

    slen = sr_fmt_bounded_len(st, 16);
    n = sr_fmt__cpy(st, slen, tmp, sizeof(tmp));
    if(n + 1u < sizeof(tmp)) {
        tmp[n++] = ' ';
        tmp[n] = '\0';
    }
    n += sr_fmt__cpy(dur, sr_fmt_bounded_len(dur, sizeof(dur)), tmp + n, sizeof(tmp) - n);

    if(radio_rev != 0u) {
        if(n + 1u < sizeof(tmp)) {
            tmp[n++] = ' ';
            tmp[n] = '\0';
        }
        if(radio_wifi != 0u && radio_ble != 0u) {
            n += sr_fmt__cpy("W+B", 3, tmp + n, sizeof(tmp) - n);
        } else if(radio_wifi != 0u) {
            n += sr_fmt__cpy("W", 1, tmp + n, sizeof(tmp) - n);
        } else if(radio_ble != 0u) {
            n += sr_fmt__cpy("B", 1, tmp + n, sizeof(tmp) - n);
        } else {
            n += sr_fmt__cpy("--", 2, tmp + n, sizeof(tmp) - n);
        }
    }

    return sr_fmt_fit(tmp, n, (size_t)SR_VIEW_COLS, out, cap);
}

/*
 * Join two bounded strings into a single display row.
 *
 *   both a and b non-empty -> "a b"
 *   only one non-empty  -> that one
 *   both empty          -> "?" (following the existing convention of field_or_q in scenes/scene_probe.c:6-8)
 *
 * b may be NULL or b_cap == 0, for rows carrying a single field (such as the hardware model).
 * Only [p, p + bounded_len) is guaranteed readable on a and b; handle per ADR-010 and never scan to a terminator.
 * Sanitizing and truncation reuse sr_fmt_fit. Returns the written length (excluding the NUL).
 */
static inline size_t sr_fmt_fw_pair(
    const char* a,
    size_t a_cap,
    const char* b,
    size_t b_cap,
    size_t max_cols,
    char* out,
    size_t out_cap) {
    char tmp[80];
    size_t n;
    size_t alen;
    size_t blen;

    if(out == NULL || out_cap == 0) {
        return 0;
    }

    alen = sr_fmt_bounded_len(a, a_cap);
    blen = sr_fmt_bounded_len(b, b_cap);

    if(alen == 0 && blen == 0) {
        return sr_fmt_fit("?", 1, max_cols, out, out_cap);
    }

    n = 0;
    tmp[0] = '\0';
    if(alen > 0) {
        n = sr_fmt__cpy(a, alen, tmp, sizeof(tmp));
    }
    if(alen > 0 && blen > 0 && n + 1u < sizeof(tmp)) {
        tmp[n++] = ' ';
        tmp[n] = '\0';
    }
    if(blen > 0) {
        n += sr_fmt__cpy(b, blen, tmp + n, sizeof(tmp) - n);
    }
    return sr_fmt_fit(tmp, n, max_cols, out, out_cap);
}

/*
 * Dash headline for the F2 capture-health verdict.
 *
 * Card §1.4 examples use CJK / emoji. FontSecondary is a `_tr` bitmap and
 * cannot draw those glyphs; the strings below keep the frozen semantics
 * (OK / WARN / CRIT / Acquiring + topd hint) in ASCII. The 23-char OK
 * string no longer fits SR_VIEW_COLS=20 (F2 rev2 §3); the caller fits it
 * against SR_HEALTH_COLS_MAX (views/sr_view_dash.h) instead, still through
 * sr_fmt_fit.
 *
 * unseen (qual_rev==0) is not "all zeros" — that would look like no-fix.
 */
static inline const char* sr_health_topd_hint(uint8_t reason) {
    if(reason == (uint8_t)SrHealthReasonGps) {
        return "gps";
    }
    if(reason == (uint8_t)SrHealthReasonSdDrop) {
        return "sd";
    }
    if(reason == (uint8_t)SrHealthReasonLink) {
        return "link";
    }
    if(reason == (uint8_t)SrHealthReasonQueue) {
        return "queue";
    }
    if(reason == (uint8_t)SrHealthReasonScan) {
        return "scan";
    }
    if(reason == (uint8_t)SrHealthReasonDedup) {
        return "dedup";
    }
    return "?";
}

/*
 * F2 rev2 §1B. How long a Qual: snapshot stays on screen before the
 * headline degrades to the unknown state. 3xT of the §1A refresh period
 * (5000 ms), chosen to tolerate two missed cycles before declaring stale.
 * Card-frozen; do not retune here.
 */
enum { SR_QUAL_STALE_MS = 15000 };

/*
 * F2 rev2 §1A. Dash tick period for resending `info`, in ticks of
 * SR_TICK_PERIOD_MS (100 ms). 5000 / 100 = 50. Shared here so host tests
 * pin the production value instead of mirroring a magic 50u (E-2).
 * This header must not include sigroam.h (ADR-003); scenes/scene_dash.c
 * _Static_assert's the 5000/SR_TICK_PERIOD_MS identity.
 */
enum { SR_QUAL_REFRESH_PERIOD_TICKS = 50 };

/*
 * F2 rev2 §1B / §1C. Pure freshness predicate for the Dash render gate.
 * Folds three conditions that all lead to the same "- need SigRoam Qual"
 * unknown-state branch of sr_view_fmt_health, so there is exactly one
 * source of truth for "should the verdict be painted right now":
 *   - qual_rev == 0        : Qual: has never been seen (generic Marauder).
 *   - sess_ms == 0          : the board's own Sess: line says no session is
 *                             running (§1C; s_sess_snap boot-accumulated
 *                             Qual: would otherwise misread as a verdict).
 *   - now - qual_tick_ms > SR_QUAL_STALE_MS : superannuated (§1B).
 * now / qual_tick_ms are furi ticks; the subtraction is unsigned and wraps
 * correctly (same convention as scene_dash.c:78-79 / sr_model.c:179-182).
 */
static inline bool sr_fmt_qual_fresh(
    uint32_t qual_rev, uint32_t qual_tick_ms, uint32_t sess_ms, uint32_t now) {
    if(qual_rev == 0u) {
        return false;
    }
    if(sess_ms == 0u) {
        return false;
    }
    if((now - qual_tick_ms) > (uint32_t)SR_QUAL_STALE_MS) {
        return false;
    }
    return true;
}

/*
 * F2 rev2 §1A. Pure periodicity decision for the Dash tick chain's Qual
 * refresh (dash_qual_refresh_tick in scenes/scene_dash.c): given the
 * app-wide tick counter (app->tick_n, one per SR_TICK_PERIOD_MS) and the
 * desired period expressed in ticks, decide whether this tick should
 * resend `info`. Generic on purpose -- sigroam.h's SR_TICK_PERIOD_MS is a
 * furi-adjacent constant and this file must not include it (ADR-003); the
 * caller converts ms to ticks itself.
 * period_ticks == 0 never fires (defensive; the real caller always passes
 * a nonzero compile-time constant).
 */
static inline bool sr_qual_refresh_due(uint32_t tick_n, uint32_t period_ticks) {
    if(period_ticks == 0u) {
        return false;
    }
    return (tick_n % period_ticks) == 0u;
}

/*
 * T6.5 half B. Dash BLE field (the value after "BLE=").
 * radio_rev == 0: Radio: never seen — unknown, print the live count, never "OFF".
 * radio_rev != 0 && radio_ble == 0: permission bit off — "OFF", not "0".
 * else: live count (BLE on; indoor ap_ble==0 is still a count).
 */
static inline size_t sr_fmt_ble_field(
    uint32_t radio_rev, uint8_t radio_ble, uint32_t ap_ble, char* out, size_t cap) {
    if(out == NULL || cap == 0) {
        return 0;
    }
    if(radio_rev != 0u && radio_ble == 0u) {
        return sr_fmt__cpy("OFF", 3, out, cap);
    }
    return sr_fmt__udec(ap_ble, out, cap);
}

static inline size_t sr_view_fmt_health(
    const SrHealthEval* e,
    const SrQualInfo* q,
    bool fresh,
    char* out,
    size_t cap) {
    char tmp[40];
    size_t n;
    const char* hint;
    uint32_t fixpct_disp;

    if(out == NULL || cap == 0) {
        return 0;
    }
    if(!fresh || e == NULL) {
        return sr_fmt__cpy("- need SigRoam Qual", 19, out, cap);
    }

    /*
     * F2 rev2 §4 / gate item 7: gps_task.c write-seq vs store_task.c
     * read-seq can hand a Qual: line where ggafix > gga, so
     * sr_capture_health_eval's fixpct (frozen, not clamped there by
     * design) can exceed 100. Clamp only for display -- the eval layer's
     * WARN-low-fix / drop-rate branch selection already ran on the
     * unclamped value and must not change.
     */
    fixpct_disp = (uint32_t)e->fixpct;
    if(fixpct_disp > 100u) {
        fixpct_disp = 100u;
    }

    tmp[0] = '\0';
    n = 0;
    if(e->v == SrHealthCrit) {
        n = sr_fmt__cpy("SD not writing", 14, tmp, sizeof(tmp));
    } else if(e->v == SrHealthAcquiring) {
        n = sr_fmt__cpy("acquiring", 9, tmp, sizeof(tmp));
    } else if(e->v == SrHealthOk) {
        n = sr_fmt__cpy("OK fix", 6, tmp, sizeof(tmp));
        n += sr_fmt__udec(fixpct_disp, tmp + n, sizeof(tmp) - n);
        if(n + 2u < sizeof(tmp)) {
            tmp[n++] = '%';
            tmp[n++] = ' ';
            tmp[n] = '\0';
        }
        n += sr_fmt__udec(q != NULL ? q->drop : 0u, tmp + n, sizeof(tmp) - n);
        n += sr_fmt__cpy("drop ", 5, tmp + n, sizeof(tmp) - n);
        n += sr_fmt__udec(q != NULL ? q->net : 0u, tmp + n, sizeof(tmp) - n);
        n += sr_fmt__cpy("net", 3, tmp + n, sizeof(tmp) - n);
    } else if(e->reason == (uint8_t)SrHealthReasonNoFix) {
        n = sr_fmt__cpy("WARN nofix sky", 14, tmp, sizeof(tmp));
    } else if(e->reason == (uint8_t)SrHealthReasonLowFix) {
        n = sr_fmt__cpy("WARN fix", 8, tmp, sizeof(tmp));
        n += sr_fmt__udec(fixpct_disp, tmp + n, sizeof(tmp) - n);
        if(n + 1u < sizeof(tmp)) {
            tmp[n++] = '%';
            tmp[n] = '\0';
        }
    } else {
        hint = sr_health_topd_hint(e->reason);
        n = sr_fmt__cpy("WARN drop ", 10, tmp, sizeof(tmp));
        n += sr_fmt__cpy(hint, sr_fmt_bounded_len(hint, 8), tmp + n, sizeof(tmp) - n);
    }
    return sr_fmt__cpy(tmp, n, out, cap);
}
