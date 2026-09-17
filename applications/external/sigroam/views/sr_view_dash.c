#include "sr_view_dash.h"

#include "../sigroam.h"
#include "../src/sr_resync.h"

#include <gui/elements.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sr_fmt_session_label maps by numeric value (the ★ layer cannot see SrSessionState).
 * Inserting a value into the middle of the enum would silently display the wrong state on
 * screen; this stops it at compile time. */
_Static_assert(
    (int)SrSessionIdle == 0 && (int)SrSessionRunning == 1 && (int)SrSessionStopped == 2,
    "SrSessionState values changed; the sr_fmt_session_label mapping must be updated to match");

/* Tab bar height: y = 0..10, with the content area starting at 11 (UI-SPEC section 3).
 * draw_tabs and draw_stream each hardcoded 11 before; T4.11 collapsed them into one constant. */
enum { SR_VIEW_TAB_H = 11 };

struct SrViewDash {
    View* view;
    SrViewDashCallback cb;
    void* ctx;
    SrViewDashCallback ok_cb;
    void* ok_ctx;
};

static void sr_view_dash_draw_tabs(Canvas* canvas, uint8_t cur) {
    static const char* const labels[SR_VIEW_TAB_COUNT] = {"Dash", "Strm", "GPS", "Sess"};
    int32_t x = 0;
    uint8_t i;
    const int32_t baseline = 9;
    const size_t box_h = (size_t)SR_VIEW_TAB_H;

    canvas_set_font(canvas, FontSecondary);
    for(i = 0; i < (uint8_t)SR_VIEW_TAB_COUNT; i++) {
        uint16_t w = canvas_string_width(canvas, labels[i]);
        if(i == cur) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, x, 0, (size_t)w + 3u, box_h);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, x + 1, baseline, labels[i]);
        x += (int32_t)w + 6;
    }
    canvas_set_color(canvas, ColorBlack);
}

/*
 * Draw a multi-line hint and return the next usable y.
 * The return value must not be ignored: the caller advances by the actual number of rows and
 * must not assume a fixed two-row jump -- a fixed 20 would push the four statistics rows to
 * y=40/50/60/70, and the last baseline of 70 > SR_CANVAS_H(64) runs off screen
 * (defect D1, confirmed by on-screen observation during B stage on 2026-08-19).
 */
static int32_t sr_view_dash_draw_hint(Canvas* canvas, int32_t y, const char* s) {
    char buf[SR_VIEW_COLS + 1];
    size_t n = 0;
    int32_t row = y;
    const char* p;

    if(s == NULL) {
        return y;
    }
    for(p = s;; p++) {
        if(*p == '\n' || *p == '\0') {
            buf[n] = '\0';
            canvas_draw_str(canvas, 0, row, buf);
            row += 10;
            n = 0;
            if(*p == '\0') {
                break;
            }
        } else if(n < (size_t)SR_VIEW_COLS) {
            buf[n++] = *p;
        }
    }
    return row;
}

static void sr_view_dash_put_line(Canvas* canvas, int32_t y, const char* raw, int n, size_t raw_cap) {
    char line[SR_VIEW_COLS + 1];
    size_t len;

    if(n < 0) {
        len = 0;
    } else {
        len = (size_t)n;
    }
    if(raw_cap > 0 && len >= raw_cap) {
        len = raw_cap - 1u;
    }
    sr_fmt_fit(raw, len, (size_t)SR_VIEW_COLS, line, sizeof(line));
    canvas_draw_str(canvas, 0, y, line);
}

/*
 * Big-number row. **The only place in this file where FontBigNumbers is allowed** --
 * `make font_guard` blocks it anywhere else.
 *
 * Three hard constraints (T4.11 / ADR-024, all verified by reading, not inferred):
 *  (1) FontBigNumbers = u8g2 `profont22_tn`, and the `_n` suffix means **numbers only**
 *      (official u8g2 wiki) -> it can draw digits only; letters come out blank. Labels must be
 *      drawn separately with FontSecondary.
 *  (2) The font height has **no authoritative source** (searches of the Flipper and u8g2 docs
 *      found no measured value) -> hardcoding is forbidden; read it at runtime with
 *      canvas_current_font_height() (available on both targets, returning size_t).
 *  (3) FontSecondary **must be restored in pairs**: all four tab content areas inherit the one
 *      font set by draw_tabs (UI-SPEC section 2), and failing to restore it contaminates
 *      subsequent tabs. A static guard cannot prove this; the real criterion is the B-stage
 *      observation B4.
 *
 * The baseline is computed from the font height (tab_h + h) rather than supplied by the
 * caller: the big font is far taller than the small one, and reusing the small font's 20 would
 * push the cap height over the tab bar or even off the top of the screen.
 * Returns the next usable baseline; returns -1 when an abnormal font height leaves no room, and
 * the caller falls back to the small-font path.
 */
static int32_t sr_view_dash_put_big(Canvas* canvas, uint32_t v, const char* label) {
    char num[12];
    size_t h;
    uint16_t wnum;
    uint16_t wlab;
    int32_t baseline;

    sr_fmt__udec(v, num, sizeof(num));

    canvas_set_font(canvas, FontBigNumbers);
    h = canvas_current_font_height(canvas);
    baseline = (int32_t)SR_VIEW_TAB_H + (int32_t)h;
    if(baseline > 61) {
        /* Font height beyond expectations (no authoritative source, so defend): give up on the
         * big font and let the caller take the small-font path. */
        canvas_set_font(canvas, FontSecondary);
        return -1;
    }
    canvas_draw_str(canvas, 0, baseline, num);
    wnum = canvas_string_width(canvas, num);

    /* The restore must happen before drawing labels: labels are letters, which BigNumbers cannot draw. */
    canvas_set_font(canvas, FontSecondary);
    wlab = canvas_string_width(canvas, label);
    if((int32_t)wnum + 3 + (int32_t)wlab <= (int32_t)SR_CANVAS_W) {
        canvas_draw_str(canvas, (int32_t)wnum + 3, baseline, label);
    }
    return baseline + 10;
}

static void sr_view_dash_draw_dash(Canvas* canvas, const SrDashModel* m) {
    char a[12];
    char b[12];
    char c[12];
    char raw[40];
    int n;
    int32_t y = 20;
    bool big;

    /* Five mutually exclusive states, evaluated in this order (T4.10 / D0-5; the Busy state was
     * added during the acceptance review on 2026-08-31). */
    if(!m->serial_open) {
        /*
         * State 1: serial not open. Hint plus small-font rows, losing nothing relative to before.
         * NOTE: **no big font here**: with the serial closed uniq is always 0, and a giant "0" is
         * worth less than a clearly worded hint.
         * The acceptance review on 2026-08-31 removed the `big = m->serial_open && !m->debug_rows;`
         * line that had been copied in -- inside the !serial_open branch it is always false, so it
         * was dead code equivalent to taking the small-font path directly.
         */
        y = sr_view_dash_draw_hint(canvas, y, sigroam_io_status_hint((SrIoStatus)m->io_status));

        /* First small-font row: uniq is the core F3 metric, so it goes on row 1. */
        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->unique_est, a, sizeof(a));
        sr_fmt__udec(m->with_gps_fix, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "uniq=%s fix=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->ap_wifi, a, sizeof(a));
        sr_fmt__udec(m->ap_ble, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "AP=%s BLE=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        /*
         * The rx row. NOTE: rx= is not a debug row: it is the only user-visible reading that tells
         * whether the link is still alive, and the entire on-device observation of defect D10 relied
         * on it (docs/HANDOFF.md:1131). Demoting it would invalidate existing SOPs.
         */
        if(y > 61) {
            return;
        }
        sr_fmt_bytes(m->rx_bytes, a, sizeof(a));
        sr_fmt_duration(m->elapsed_ms, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "rx=%s %s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(!m->debug_rows) {
            return;
        }

        /* The next two rows are drawn only when Debug rows = On in Settings (for T5.1 long-run evidence). */
        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->rx_dropped, a, sizeof(a));
        sr_fmt__udec(m->rx_max_fill, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "d=%s f=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->heap_free / 1024u, a, sizeof(a));
        sr_fmt__udec(m->heap_min / 1024u, b, sizeof(b));
        sr_fmt__udec(m->heap_max_blk / 1024u, c, sizeof(c));
        n = snprintf(raw, sizeof(raw), "h=%sK m=%sK b=%sK", a, b, c);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        return;
    }

    if(m->wait_stage == (uint8_t)SR_RESYNC_HINT_BUSY ||
       m->wait_stage == (uint8_t)SR_RESYNC_HINT_LOST) {
        /* Packed into wait_stage so SrDashModel does not grow (sizeof == 644).
         * Single line: a two-line hint pushes the Dash tab's fourth row off screen (D1). */
        if(y > 61) {
            return;
        }
        if(m->wait_stage == (uint8_t)SR_RESYNC_HINT_BUSY) {
            n = snprintf(raw, sizeof(raw), "Resyncing...");
        } else {
            n = snprintf(raw, sizeof(raw), "Scan lost, press OK");
        }
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt_bytes(m->rx_bytes, a, sizeof(a));
        n = snprintf(raw, sizeof(raw), "rx=%s", a);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        /*
         * Resync diagnostics row. **Not** gated on debug_rows: the 5.4 unplug SOP needs it and
         * Settings is not reachable mid-run. All three fields already exist in SrDashModel, so
         * sizeof stays 644 (test_gps_sample.c:723).
         *
         * Why exactly these three. sr_resync.h reaches SrResyncLost through exactly two exits:
         * giveup at trigger_ms+30000, or AwaitStop timing out with tries >= 3 at t1+12000.
         * Session state alone separates them:
         *   Running -> the first stopscan was never confirmed (all three tries failed);
         *              Lost is the tries>=3 exit and the AwaitStart path never ran.
         *   Stopped -> step 1 succeeded and we were parked in AwaitStart, so a Lost earlier
         *              than 30 s would mean a real hole in that path.
         * i (illegal_trans) then splits "the peer never answered" from "the peer answered but
         * sr_model refused the transition" (src/sr_model.c:135-170: apply_started while already
         * Running, and apply_stopped while not Running, both only bump illegal_trans).
         * r (session_rev) is the absolute counter -- record it once before unplugging.
         */
        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->session_rev, a, sizeof(a));
        sr_fmt__udec(m->illegal_trans, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "%s r=%s i=%s", sr_fmt_session_label(m->session), a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(!m->debug_rows) {
            return;
        }

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->rx_dropped, a, sizeof(a));
        sr_fmt__udec(m->rx_max_fill, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "d=%s f=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->heap_free / 1024u, a, sizeof(a));
        sr_fmt__udec(m->heap_min / 1024u, b, sizeof(b));
        sr_fmt__udec(m->heap_max_blk / 1024u, c, sizeof(c));
        n = snprintf(raw, sizeof(raw), "h=%sK m=%sK b=%sK", a, b, c);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        return;
    }

    if(m->wait_stage != (uint8_t)SrWaitStageNone) {
        /* State 2: command in progress. No big font. rx shows bytes only, without duration. */
        if(y > 61) {
            return;
        }
        n = snprintf(raw, sizeof(raw), "Waiting...");
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        if(m->wait_stage == (uint8_t)SrWaitStageLink) {
            n = snprintf(raw, sizeof(raw), "for board data");
        } else if(m->wait_stage == (uint8_t)SrWaitStageCmd) {
            n = snprintf(raw, sizeof(raw), "for cmd accepted");
        } else if(m->wait_stage == (uint8_t)SrWaitStageFunc && m->cmd_is_start) {
            n = snprintf(raw, sizeof(raw), "for scan to start");
        } else {
            n = snprintf(raw, sizeof(raw), "for scan to stop");
        }
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt_bytes(m->rx_bytes, a, sizeof(a));
        n = snprintf(raw, sizeof(raw), "rx=%s", a);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(!m->debug_rows) {
            return;
        }

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->rx_dropped, a, sizeof(a));
        sr_fmt__udec(m->rx_max_fill, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "d=%s f=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->heap_free / 1024u, a, sizeof(a));
        sr_fmt__udec(m->heap_min / 1024u, b, sizeof(b));
        sr_fmt__udec(m->heap_max_blk / 1024u, c, sizeof(c));
        n = snprintf(raw, sizeof(raw), "h=%sK m=%sK b=%sK", a, b, c);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        return;
    }

    if(m->session == (uint8_t)SrSessionRunning) {
        /* State 3: running. Big-font uniq + AP=/BLE=/fix= + rx= with duration; unchanged from before. */
        /*
         * The big font is drawn only when the serial is open AND debug rows are off:
         *  - with the serial closed uniq is always 0, and a giant "0" is worth less than a clear hint;
         *  - with debug rows on it would be the big font plus 5 small rows, which does not fit the
         *    53 px content area.
         * Both cases fall back to the small-font path, losing nothing.
         */
        big = m->serial_open && !m->debug_rows;
        if(big) {
            int32_t next = sr_view_dash_put_big(canvas, m->unique_est, "uniq");
            if(next < 0) {
                big = false; /* Abnormal font height; defensively fall back to the small font */
            } else {
                y = next;
            }
        }

        if(!big) {
            /* First small-font row: uniq moves up from row 2 to row 1 (it is the core F3 metric). */
            if(y > 61) {
                return;
            }
            sr_fmt__udec(m->unique_est, a, sizeof(a));
            sr_fmt__udec(m->with_gps_fix, b, sizeof(b));
            n = snprintf(raw, sizeof(raw), "uniq=%s fix=%s", a, b);
            sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
            y += 10;

            if(y > 61) {
                return;
            }
            sr_fmt__udec(m->ap_wifi, a, sizeof(a));
            sr_fmt__udec(m->ap_ble, b, sizeof(b));
            n = snprintf(raw, sizeof(raw), "AP=%s BLE=%s", a, b);
            sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
            y += 10;
        } else {
            /* Big-font mode: AP / BLE / fix share one row (measured: "AP=99 BLE=99 fix=99" = 19 <= 20). */
            if(y > 61) {
                return;
            }
            sr_fmt__udec(m->ap_wifi, a, sizeof(a));
            sr_fmt__udec(m->ap_ble, b, sizeof(b));
            sr_fmt__udec(m->with_gps_fix, c, sizeof(c));
            n = snprintf(raw, sizeof(raw), "AP=%s BLE=%s fix=%s", a, b, c);
            sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
            y += 10;
        }

        /*
         * The rx row. NOTE: rx= is not a debug row: it is the only user-visible reading that tells
         * whether the link is still alive, and the entire on-device observation of defect D10 relied
         * on it (docs/HANDOFF.md:1131). Demoting it would invalidate existing SOPs.
         */
        if(y > 61) {
            return;
        }
        sr_fmt_bytes(m->rx_bytes, a, sizeof(a));
        sr_fmt_duration(m->elapsed_ms, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "rx=%s %s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(!m->debug_rows) {
            return;
        }

        /* The next two rows are drawn only when Debug rows = On in Settings (for T5.1 long-run evidence). */
        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->rx_dropped, a, sizeof(a));
        sr_fmt__udec(m->rx_max_fill, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "d=%s f=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->heap_free / 1024u, a, sizeof(a));
        sr_fmt__udec(m->heap_min / 1024u, b, sizeof(b));
        sr_fmt__udec(m->heap_max_blk / 1024u, c, sizeof(c));
        n = snprintf(raw, sizeof(raw), "h=%sK m=%sK b=%sK", a, b, c);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        return;
    }

    if(m->scan_ui == (uint8_t)SrScanUiBusy) {
        /*
         * Busy state: the command was never sent at all -- sr_worker_send_cmd returned false,
         * meaning the command slot still holds a previous unsent command (src/sr_worker.h:32-37).
         *
         * No extra Busy-only field: this reuses the existing m->scan_ui -- sr_scan_ctl_eval returns
         * exactly SrScanUiBusy on cmd_rejected (src/sr_scan_ctl.h:48-50), and dash_fill already
         * fills it in. D12 added uint8_t gps_src; sizeof(SrDashModel) stayed 644 (the byte fit in
         * existing padding). Layout sentinels at tools/host_test/test_gps_sample.c:722-724 are
         * only to be edited when the main session authorizes each assertion by line.
         *
         * Why stating the cause outright is allowed (and does not violate ADR-022 decision 4): this
         * is a **local fact**, derived directly from our own queue state, not a conclusion about the
         * device inferred from "nothing observed for a while".
         *
         * Placed after state 3: while running, preserving the big-font uniq capture reading takes
         * priority -- if a stop never went out the screen still reads RUNNING and the user just
         * presses again; whereas the real ailment this state treats (pressing OK repeatedly in the
         * ready state with no feedback at all) sits at state 4's position and is caught right here.
         *
         * NOTE: cmd_rejected is sticky: only the next successful send clears it (dash_queue_cmd in
         * scenes/scene_dash.c). So the wording must offer an actionable next step, not merely report
         * a state.
         */
        if(y > 61) {
            return;
        }
        n = snprintf(raw, sizeof(raw), "Command not sent");
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        n = snprintf(raw, sizeof(raw), "press OK again");
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt_bytes(m->rx_bytes, a, sizeof(a));
        n = snprintf(raw, sizeof(raw), "rx=%s", a);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(!m->debug_rows) {
            return;
        }

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->rx_dropped, a, sizeof(a));
        sr_fmt__udec(m->rx_max_fill, b, sizeof(b));
        n = snprintf(raw, sizeof(raw), "d=%s f=%s", a, b);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        y += 10;

        if(y > 61) {
            return;
        }
        sr_fmt__udec(m->heap_free / 1024u, a, sizeof(a));
        sr_fmt__udec(m->heap_min / 1024u, b, sizeof(b));
        sr_fmt__udec(m->heap_max_blk / 1024u, c, sizeof(c));
        n = snprintf(raw, sizeof(raw), "h=%sK m=%sK b=%sK", a, b, c);
        sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
        return;
    }

    /* State 4: readiness. Serial open, no pending command, not scanning. No big font. rx without duration. */
    if(y > 61) {
        return;
    }
    if(m->rx_bytes == 0u) {
        n = snprintf(raw, sizeof(raw), "Board: no data");
    } else {
        n = snprintf(raw, sizeof(raw), "Board: data ok");
    }
    sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
    y += 10;

    if(y > 61) {
        return;
    }
    n = snprintf(raw, sizeof(raw), "OK: Start scan");
    sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
    y += 10;

    if(y > 61) {
        return;
    }
    sr_fmt_bytes(m->rx_bytes, a, sizeof(a));
    n = snprintf(raw, sizeof(raw), "rx=%s", a);
    sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
    y += 10;

    if(!m->debug_rows) {
        return;
    }

    if(y > 61) {
        return;
    }
    sr_fmt__udec(m->rx_dropped, a, sizeof(a));
    sr_fmt__udec(m->rx_max_fill, b, sizeof(b));
    n = snprintf(raw, sizeof(raw), "d=%s f=%s", a, b);
    sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
    y += 10;

    if(y > 61) {
        return;
    }
    sr_fmt__udec(m->heap_free / 1024u, a, sizeof(a));
    sr_fmt__udec(m->heap_min / 1024u, b, sizeof(b));
    sr_fmt__udec(m->heap_max_blk / 1024u, c, sizeof(c));
    n = snprintf(raw, sizeof(raw), "h=%sK m=%sK b=%sK", a, b, c);
    sr_view_dash_put_line(canvas, y, raw, n, sizeof(raw));
}

/* Four cells: x=0,3,6,9, each 2 px wide, heights 2/4/6/8. Zero cells draw a 2x1 base line, which stays distinguishable from full. */
static void sr_view_dash_draw_rssi_bars(Canvas* canvas, int32_t x, int32_t baseline, uint8_t n) {
    static const uint8_t heights[4] = {2, 4, 6, 8};
    uint8_t i;

    if(n == 0) {
        canvas_draw_box(canvas, x, baseline - 1, 2, 1);
        return;
    }
    if(n > 4) {
        n = 4;
    }
    for(i = 0; i < n; i++) {
        uint8_t h = heights[i];
        canvas_draw_box(canvas, x + (int32_t)i * 3, baseline - (int32_t)h, 2, (size_t)h);
    }
}

static void sr_view_dash_draw_stream(Canvas* canvas, const SrDashModel* m) {
    char buf[SR_STREAM_COLS + 1];
    uint8_t i;
    int32_t y;
    const int32_t tab_h = SR_VIEW_TAB_H;

    if(m->stream_count == 0) {
        if(!m->serial_open) {
            sr_view_dash_draw_hint(canvas, 20, sigroam_io_status_hint((SrIoStatus)m->io_status));
        } else {
            canvas_draw_str(canvas, 0, 21, "No APs yet");
        }
        return;
    }

    for(i = 0; i < m->stream_n && i < (uint8_t)SR_STREAM_ROWS; i++) {
        const SrApBrief* r = &m->stream_rows[i];
        size_t slen;

        y = 21 + (int32_t)i * 10;
        sr_view_dash_draw_rssi_bars(canvas, 0, y, sr_fmt_rssi_bars(r->rssi));
        slen = sr_fmt_bounded_len(r->ssid, sizeof(r->ssid));
        sr_fmt_stream_row(
            r->ssid,
            slen,
            (r->flags & SR_AP_FLAG_BLE) != 0,
            r->channel,
            (size_t)SR_STREAM_COLS,
            buf,
            sizeof(buf));
        canvas_draw_str(canvas, 12, y, buf);
    }

    if(m->stream_count > (uint16_t)SR_STREAM_ROWS) {
        elements_scrollbar_pos(
            canvas,
            (int32_t)SR_CANVAS_W - 3,
            tab_h,
            (size_t)((int32_t)SR_CANVAS_H - tab_h),
            (size_t)m->stream_top,
            (size_t)m->stream_count);
    }
}

/* During an active wardrive, dash_fill stores POI phase/gate in gps_phase/gps_gate
 * so this existing fifth-line slot can show POI copy without a new SrDashModel field
 * (sizeof pinned at 644). Idle GPS sampling still uses sr_gps_status_text. */
static const char* sr_view_dash_gps_hint(const SrDashModel* m, bool idle_hint) {
    if(m->session == (uint8_t)SrSessionRunning &&
       m->scan_ui == (uint8_t)SrScanUiRunning) {
        return sr_poi_status_text(m->gps_phase, m->gps_gate);
    }
    return sr_gps_status_text(m->gps_phase, m->gps_gate, idle_hint);
}

static void sr_view_dash_draw_gps(Canvas* canvas, const SrDashModel* m) {
    char raw[64];
    char a[SR_VIEW_COLS + 1];
    char b[SR_VIEW_COLS + 1];
    int n;
    size_t slen;
    const char* s;

    if(m->gps_src == 0) {
        if(!m->serial_open) {
            int32_t y = sr_view_dash_draw_hint(
                canvas, 20, sigroam_io_status_hint((SrIoStatus)m->io_status));
            s = sr_view_dash_gps_hint(m, true);
            if(s != NULL) {
                canvas_draw_str(canvas, 0, y, s);
            }
        } else {
            canvas_draw_str(canvas, 0, 21, "No GPS data yet");
            s = sr_view_dash_gps_hint(m, true);
            if(s != NULL) {
                canvas_draw_str(canvas, 0, 31, s);
            }
        }
        return;
    }

    if(m->gps_src == 2) {
        n = snprintf(raw, sizeof(raw), "Fix: %s  (live)", m->gps.fix ? "Yes" : "No");
    } else {
        slen = sr_fmt_bounded_len(m->gps.sats, sizeof(m->gps.sats));
        n = snprintf(
            raw,
            sizeof(raw),
            "Fix: %s  Sats: %.*s",
            m->gps.fix ? "Yes" : "No",
            (int)slen,
            m->gps.sats);
    }
    sr_view_dash_put_line(canvas, 21, raw, n, sizeof(raw));

    sr_fmt_gps_val(m->gps.lat, sizeof(m->gps.lat), m->gps.fix, (size_t)SR_VIEW_COLS, a, sizeof(a));
    n = snprintf(raw, sizeof(raw), "Lat: %s", a);
    sr_view_dash_put_line(canvas, 31, raw, n, sizeof(raw));

    sr_fmt_gps_val(m->gps.lon, sizeof(m->gps.lon), m->gps.fix, (size_t)SR_VIEW_COLS, a, sizeof(a));
    n = snprintf(raw, sizeof(raw), "Lon: %s", a);
    sr_view_dash_put_line(canvas, 41, raw, n, sizeof(raw));

    sr_fmt_gps_val(m->gps.alt, sizeof(m->gps.alt), m->gps.fix, (size_t)SR_VIEW_COLS, a, sizeof(a));
    sr_fmt_gps_val(m->gps.acc, sizeof(m->gps.acc), m->gps.fix, (size_t)SR_VIEW_COLS, b, sizeof(b));
    n = snprintf(raw, sizeof(raw), "Alt:%s Acc:%s", a, b);
    sr_view_dash_put_line(canvas, 51, raw, n, sizeof(raw));

    s = sr_view_dash_gps_hint(m, false);
    if(s != NULL) {
        canvas_draw_str(canvas, 0, 61, s);
    } else {
        n = (int)sr_fmt_gps_stamp(
            m->gps.datetime, sizeof(m->gps.datetime), m->gps.fix, m->gps_blocks, raw, sizeof(raw));
        sr_view_dash_put_line(canvas, 61, raw, n, sizeof(raw));
    }
}

static void sr_view_dash_draw_session(Canvas* canvas, const SrDashModel* m) {
    char raw[64];
    char a[12];
    char b[12];
    int n;

    n = snprintf(raw, sizeof(raw), "State %s", sr_fmt_session_label(m->session));
    sr_view_dash_put_line(canvas, 21, raw, n, sizeof(raw));

    sr_fmt__udec(m->session_rev, a, sizeof(a));
    sr_fmt__udec(m->illegal_trans, b, sizeof(b));
    n = snprintf(raw, sizeof(raw), "Changes %s Illegal %s", a, b);
    sr_view_dash_put_line(canvas, 31, raw, n, sizeof(raw));

    sr_fmt__udec(m->unknown_lines, a, sizeof(a));
    sr_fmt__udec(m->malformed_lines, b, sizeof(b));
    n = snprintf(raw, sizeof(raw), "Unknown %s Bad %s", a, b);
    sr_view_dash_put_line(canvas, 41, raw, n, sizeof(raw));

    if(m->firmware_rev == 0) {
        n = snprintf(raw, sizeof(raw), "Firmware unknown");
        sr_view_dash_put_line(canvas, 51, raw, n, sizeof(raw));
        n = snprintf(raw, sizeof(raw), "Use Probe firmware");
        sr_view_dash_put_line(canvas, 61, raw, n, sizeof(raw));
    } else {
        n = (int)sr_fmt_fw_pair(
            m->firmware.firmware,
            sizeof(m->firmware.firmware),
            m->firmware.version,
            sizeof(m->firmware.version),
            (size_t)SR_VIEW_COLS,
            raw,
            sizeof(raw));
        sr_view_dash_put_line(canvas, 51, raw, n, sizeof(raw));

        n = (int)sr_fmt_fw_pair(
            m->firmware.hardware,
            sizeof(m->firmware.hardware),
            NULL,
            0,
            (size_t)SR_VIEW_COLS,
            raw,
            sizeof(raw));
        sr_view_dash_put_line(canvas, 61, raw, n, sizeof(raw));
    }
}

/* draw: read the model and draw, nothing else. Does not dereference app, take locks, allocate, or do IO. */
static void sr_view_dash_draw(Canvas* canvas, void* model) {
    const SrDashModel* m = model;

    canvas_clear(canvas);
    if(m == NULL) {
        return;
    }

    sr_view_dash_draw_tabs(canvas, m->tab);

    if(m->tab == (uint8_t)SR_VIEW_TAB_DASH) {
        sr_view_dash_draw_dash(canvas, m);
        return;
    }
    if(m->tab == (uint8_t)SR_VIEW_TAB_STREAM) {
        sr_view_dash_draw_stream(canvas, m);
        return;
    }
    if(m->tab == (uint8_t)SR_VIEW_TAB_GPS) {
        sr_view_dash_draw_gps(canvas, m);
        return;
    }
    if(m->tab == (uint8_t)SR_VIEW_TAB_SESSION) {
        sr_view_dash_draw_session(canvas, m);
        return;
    }
    return;
}

static bool sr_view_dash_input(InputEvent* event, void* context) {
    SrViewDash* d = context;
    int dir;
    bool scrolled = false;

    if(event == NULL || d == NULL || d->view == NULL) {
        return false;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        bool ok_tab = false;

        with_view_model(
            d->view,
            SrDashModel* m,
            {
                if(m != NULL &&
                   (m->tab == (uint8_t)SR_VIEW_TAB_GPS ||
                    m->tab == (uint8_t)SR_VIEW_TAB_DASH)) {
                    ok_tab = true;
                }
            },
            false);
        if(ok_tab) {
            if(d->ok_cb != NULL) {
                d->ok_cb(d->ok_ctx);
            }
            return true;
        }
        return false;
    }

    if(event->key == InputKeyRight || event->key == InputKeyLeft) {
        if(event->type != InputTypeShort) {
            return false;
        }
        dir = (event->key == InputKeyRight) ? 1 : -1;
        with_view_model(
            d->view,
            SrDashModel* m,
            {
                if(m != NULL) {
                    m->tab = sr_view_tab_next(m->tab, dir);
                    m->stream_top = 0;
                }
            },
            true);
        return true;
    }

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
            return false;
        }
        dir = (event->key == InputKeyDown) ? 1 : -1;
        with_view_model(
            d->view,
            SrDashModel* m,
            {
                if(m != NULL && m->tab == (uint8_t)SR_VIEW_TAB_STREAM) {
                    m->stream_top = sr_stream_scroll(
                        m->stream_top, m->stream_count, (uint8_t)SR_STREAM_ROWS, dir);
                    scrolled = true;
                }
            },
            true);
        /* The callback must live outside the with_view_model block; otherwise it would take the app
         * lock while holding the ViewModel lock, inverting the lock order. */
        if(scrolled && d->cb != NULL) {
            d->cb(d->ctx);
        }
        return scrolled;
    }

    return false;
}

SrViewDash* sr_view_dash_alloc(void) {
    SrViewDash* d = malloc(sizeof(SrViewDash));

    furi_check(d);
    memset(d, 0, sizeof(SrViewDash));

    d->view = view_alloc();
    view_allocate_model(d->view, ViewModelTypeLocking, sizeof(SrDashModel));
    view_set_context(d->view, d);
    view_set_draw_callback(d->view, sr_view_dash_draw);
    view_set_input_callback(d->view, sr_view_dash_input);

    with_view_model(
        d->view, SrDashModel* m, { memset(m, 0, sizeof(SrDashModel)); }, false);

    return d;
}

void sr_view_dash_free(SrViewDash* d) {
    if(d == NULL) {
        return;
    }
    if(d->view) {
        view_free(d->view);
        d->view = NULL;
    }
    free(d);
}

View* sr_view_dash_get_view(SrViewDash* d) {
    return d ? d->view : NULL;
}

void sr_view_dash_set_callback(SrViewDash* d, SrViewDashCallback cb, void* context) {
    if(d == NULL) {
        return;
    }
    d->cb = cb;
    d->ctx = context;
}

void sr_view_dash_set_ok_callback(SrViewDash* d, SrViewDashCallback cb, void* context) {
    if(d == NULL) {
        return;
    }
    d->ok_cb = cb;
    d->ok_ctx = context;
}

void sr_view_dash_set(View* v, const SrDashModel* src) {
    uint8_t tab = (uint8_t)SR_VIEW_TAB_DASH;

    if(v == NULL || src == NULL) {
        return;
    }
    with_view_model(
        v,
        SrDashModel* m,
        {
            if(m != NULL) {
                tab = m->tab;
                *m = *src;
                m->tab = tab;
            }
        },
        true);
}
