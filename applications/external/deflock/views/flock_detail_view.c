// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_detail_view.h"
#include "../recon_app_i.h"
#include "../helpers/report_fmt.h"
#include "ui_widgets.h"

#include <gui/elements.h>

#include <math.h>
#include <string.h>

// Layout is fixed arithmetic rather than SDK button chrome, deliberately.
// elements_button_center() and elements_button_right() each draw a ~12px filled
// box at the bottom sized to its LABEL, so "Unmark" (centered) and "Lock In"
// (flush right) overlap horizontally at these string widths, and the boxes reach
// up into the last content row's descenders. Placing both hints by hand keeps
// the whole screen determined by numbers that can be checked without a device.
//
//   0..12   title bar (UI_TITLE_BAR_H 13)
//   13..50  content: 4 rows of 9 (text baselines 22/31/40/49, bars 13..21 etc.)
//   52      separator
//   54..63  footer hints (baseline 61 + 2px descender = 63, the last screen row)
#define CONTENT_TOP  14
#define ROW_H        9
#define VISIBLE_ROWS 4
#define SCROLLBAR_X  125
#define TEXT_X       2
#define FOOTER_LINE  52
#define FOOTER_BASE  61

struct FlockDetailView {
    View* view;
    FlockDetailActionCallback mark_cb;
    FlockDetailActionCallback lock_cb;
    void* ctx;
};

typedef struct {
    void* app; /**< ReconApp* */
    int top; /**< first visible content row */
} FlockDetailModel;

/**
 * The content rows, in display order. Built per frame as a list of KINDS rather
 * than a list of formatted strings: only the <=4 visible rows are ever rendered,
 * so one small reusable buffer replaces an array of ~11 full-width strings that
 * would otherwise sit on the GUI thread's stack every draw.
 */
typedef enum {
    FdClass,
    FdMethod,
    FdSeen,
    FdMac,
    FdSsid,
    FdRssi, /**< the one row that also draws graphical bars */
    FdCh,
    FdGps, /**< "no fix" only -- a real fix uses FdLat + FdLon */
    FdLat, /**< a fix, split across two rows: both coords on one line ran off */
    FdLon,
    FdSaved, /**< archived entries only */
    FdHidden, /**< hidden-SSID beaconing observed */
    FdIeFp, /**< a probe IE-fingerprint was captured */
    FdKindCount,
} FdLineKind;

/**
 * How this detection was framed on the air, phrased for the Method row. Kept
 * separate from the terse frame-type tag so "Method:" reads as a sentence
 * fragment ("OUI + beacon") rather than a code.
 *
 * Deliberately does NOT distinguish a hidden beacon. "Method: OUI + hidden
 * beacon" overflowed the 128 px row on hardware, and the hiding already has its
 * own dedicated line below ("Hidden SSID (not scored)") -- saying it twice cost
 * width and told the operator nothing new.
 */
static const char* fd_src_phrase(char ftype) {
    switch(ftype) {
    case 'B':
        return "beacon";
    case 'P':
    case 'F': // IE-fingerprint match -- still a probe request on the air
        return "probe req";
    case 'R':
        return "probe resp";
    case 'L':
        return "BLE advert";
    default:
        return "RF";
    }
}

/**
 * Draw `s` at (x, baseline), trimmed with a ".." marker if it would not fit
 * inside `max_x`.
 *
 * The Widget text-scroll element this screen replaced WRAPPED an over-long line
 * onto the next row; a fixed-row renderer cannot, so without this an SSID at the
 * full 32 characters would simply run off the right edge and under the
 * scrollbar. Trimming visibly is the honest degradation -- the untruncated value
 * is still in the saved report.
 */
static void fd_draw_fit(Canvas* canvas, int x, int baseline, const char* s, int max_x) {
    int avail = max_x - x;
    if(canvas_string_width(canvas, s) <= avail) {
        canvas_draw_str(canvas, x, baseline, s);
        return;
    }
    char probe[52];
    size_t n = strlen(s);
    if(n > sizeof(probe) - 3) n = sizeof(probe) - 3;
    while(n > 0) {
        n--;
        memcpy(probe, s, n);
        probe[n] = '.';
        probe[n + 1] = '.';
        probe[n + 2] = '\0';
        if(canvas_string_width(canvas, probe) <= avail) break;
    }
    canvas_draw_str(canvas, x, baseline, probe);
}

/** Format one content row. Returns true if the row should also draw signal bars. */
static bool fd_format(char* buf, size_t len, FdLineKind kind, const FlockEntry* e) {
    switch(kind) {
    case FdClass:
        // What it IS. The confidence rung in the title bar says how sure we are,
        // which is a different question.
        snprintf(buf, len, "%s", flock_class_long_str((FlockDevClass)e->dev_class));
        return false;
    case FdMethod: {
        // WHY it is on the list. "Possible" states confidence but not what
        // matched, and an OUI-prefix lead deserves very different trust from an
        // SSID pattern hit -- issue #5 asked exactly this ("What made it a
        // possible hit?"). Re-derived from stored evidence, never asserted by
        // the companion, so it cannot inherit an over-claim from older firmware.
        FlockMethod m = flock_method_of(e->mac, e->ssid, e->ftype, e->ie_fp);
        if(m == FlockMethodBle || m == FlockMethodUnknown) {
            // "BLE mfg ID + BLE advert" says the same thing twice, and an
            // "ESP probe rule" verdict is already about how it was seen -- both
            // read better alone, and both would otherwise overflow the row.
            snprintf(buf, len, "Method: %s", flock_method_str(m));
        } else {
            snprintf(buf, len, "Method: %s + %s", flock_method_str(m), fd_src_phrase(e->ftype));
        }
        return false;
    }
    case FdSeen:
        snprintf(buf, len, "Seen: %lu", (unsigned long)e->count);
        return false;
    case FdMac: {
        char mac[18];
        fmt_mac(mac, sizeof(mac), e->mac);
        snprintf(buf, len, "MAC: %s", mac);
        return false;
    }
    case FdSsid:
        // "(hidden)" has always meant "we have no name for it" here. Only say the
        // AP is actively withholding one when we watched it beacon without a name.
        snprintf(
            buf,
            len,
            "SSID: %s",
            e->ssid[0] ? e->ssid : (e->hidden ? "(withheld by AP)" : "(none seen)"));
        return false;
    case FdRssi:
        // An ARCHIVED entry's RSSI was recorded on an earlier run. Label it as
        // last-known and draw NO bars: bars are a live-signal claim, and asserting
        // the device is in range right now is the over-claim the
        // detections-are-indicators rule exists to prevent.
        snprintf(buf, len, "%s: %d", e->archived ? "Last RSSI" : "RSSI", e->rssi);
        return !e->archived;
    case FdCh:
        snprintf(buf, len, "Ch: %u", e->channel);
        return false;
    case FdGps:
        snprintf(buf, len, "GPS: no fix");
        return false;
    case FdLat:
        // One coordinate per row. "GPS: 47.62050, -122.34930" is 25 characters
        // and ran under the scrollbar off the right edge of the screen; dropping
        // to 4 decimals would have fixed the width by throwing away ~10 m of
        // precision on the one field whose whole purpose is finding the thing.
        snprintf(buf, len, "Lat: %.5f", (double)e->lat);
        return false;
    case FdLon:
        snprintf(buf, len, "Lon: %.5f", (double)e->lon);
        return false;
    case FdSaved: {
        DateTime dt;
        datetime_timestamp_to_datetime(e->seen_epoch, &dt);
        snprintf(
            buf,
            len,
            "Saved: %04u-%02u-%02u %02u:%02u",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute);
        return false;
    }
    case FdHidden:
        // An OBSERVATION, not a score: it did not raise the confidence rung, and
        // the wording must not imply that it did. Flock moved to hidden SSIDs,
        // but so do plenty of ordinary home routers.
        snprintf(buf, len, "Hidden SSID (not scored)");
        return false;
    case FdIeFp:
        // A confirmed unit's fp can be dropped into signatures.json ("ie_fps") to
        // catch its MAC-randomized twins.
        snprintf(buf, len, "IE-fp: %08lx", (unsigned long)e->ie_fp);
        return false;
    default:
        buf[0] = '\0';
        return false;
    }
}

static void flock_detail_view_draw_callback(Canvas* canvas, void* _model) {
    FlockDetailModel* model = _model;
    ReconApp* app = model->app;
    if(!app) return;

    // Snapshot the entry under the lock, then render entirely unlocked -- holding
    // app->mutex across a canvas pass stalls the ESP worker every frame (same
    // discipline as flock_view.c).
    FlockEntry e;
    bool valid;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    valid = app->selected >= 0 && app->selected < (int)app->flock_count;
    if(valid) e = app->flock[app->selected];
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);

    if(!valid) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No selection");
        return;
    }

    ui_title_bar(canvas, flock_confidence_str(e.confidence), e.marked ? "MARKED" : NULL);

    // Which rows exist for THIS entry.
    uint8_t kinds[FdKindCount];
    int n = 0;
    kinds[n++] = FdClass;
    kinds[n++] = FdMethod;
    kinds[n++] = FdSeen;
    kinds[n++] = FdMac;
    kinds[n++] = FdSsid;
    kinds[n++] = FdRssi;
    kinds[n++] = FdCh;
    if(!isnan(e.lat) && !isnan(e.lon)) {
        kinds[n++] = FdLat;
        kinds[n++] = FdLon;
    } else {
        kinds[n++] = FdGps;
    }
    // Where a stored hit came from, in wall-clock terms. Only meaningful for an
    // archived entry: a live one's seen_epoch is "moments ago" by definition.
    if(e.archived && e.seen_epoch) kinds[n++] = FdSaved;
    if(e.hidden) kinds[n++] = FdHidden;
    if(e.ie_fp != 0) kinds[n++] = FdIeFp;

    // Clamp the scroll offset here rather than in the input handler: the row count
    // changes as fields appear (a geotag lands, an IE-fp arrives), so a offset
    // that was valid last frame may not be now.
    int max_top = n - VISIBLE_ROWS;
    if(max_top < 0) max_top = 0;
    if(model->top > max_top) model->top = max_top;
    if(model->top < 0) model->top = 0;

    // Content stops short of the scrollbar only when one is actually drawn.
    bool scrolls = (n > VISIBLE_ROWS);
    int max_x = scrolls ? SCROLLBAR_X - 2 : 126;

    canvas_set_font(canvas, FontSecondary);
    char buf[48];
    for(int row = 0; row < VISIBLE_ROWS; row++) {
        int idx = model->top + row;
        if(idx >= n) break;
        int ry = CONTENT_TOP + row * ROW_H;
        bool bars = fd_format(buf, sizeof(buf), (FdLineKind)kinds[idx], &e);
        if(bars) {
            // Same helper, same scale as the list rows one screen back -- that
            // consistency is the whole point of this view existing. Reserve the
            // bar cell so a long label can never be drawn through it.
            int bx = TEXT_X + canvas_string_width(canvas, buf) + 4;
            fd_draw_fit(canvas, TEXT_X, ry + 8, buf, max_x - 15);
            ui_signal_bars(canvas, bx, ry - 1, e.rssi);
        } else {
            fd_draw_fit(canvas, TEXT_X, ry + 8, buf, max_x);
        }
    }

    if(scrolls) {
        elements_scrollbar_pos(
            canvas,
            SCROLLBAR_X,
            CONTENT_TOP,
            VISIBLE_ROWS * ROW_H,
            (size_t)model->top,
            (size_t)max_top + 1);
    }

    // Footer: which key does what. Left hint is the primary action (OK), right
    // hint is "Lock In" (issue #6) -- hold the companion on this one device and
    // stream live RSSI, so a hit can be walked/driven down instead of staying a
    // MAC on a screen. Opposite edges, so the two can never collide.
    canvas_draw_line(canvas, 0, FOOTER_LINE, 128, FOOTER_LINE);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, TEXT_X, FOOTER_BASE, e.marked ? "OK Unmark" : "OK Mark");
    canvas_draw_str_aligned(canvas, 126, FOOTER_BASE, AlignRight, AlignBottom, "Lock In >");
}

static bool flock_detail_view_input_callback(InputEvent* event, void* context) {
    FlockDetailView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(
            v->view,
            FlockDetailModel * model,
            {
                if(model->top > 0) model->top--;
            },
            true);
        return true;
    case InputKeyDown:
        // The draw pass owns the upper clamp (it knows how many rows this entry
        // has); incrementing past the end here is corrected before anything paints.
        with_view_model(v->view, FlockDetailModel * model, { model->top++; }, true);
        return true;
    case InputKeyOk:
        if(event->type == InputTypeShort && v->mark_cb) v->mark_cb(v->ctx);
        return true;
    case InputKeyRight:
        if(event->type == InputTypeShort && v->lock_cb) v->lock_cb(v->ctx);
        return true;
    default:
        return false; // Back bubbles up to the scene manager
    }
}

FlockDetailView* flock_detail_view_alloc(void) {
    FlockDetailView* v = malloc(sizeof(FlockDetailView));
    v->mark_cb = NULL;
    v->lock_cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(FlockDetailModel));
    view_set_draw_callback(v->view, flock_detail_view_draw_callback);
    view_set_input_callback(v->view, flock_detail_view_input_callback);
    with_view_model(
        v->view,
        FlockDetailModel * model,
        {
            model->app = NULL;
            model->top = 0;
        },
        false);
    return v;
}

void flock_detail_view_free(FlockDetailView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* flock_detail_view_get_view(FlockDetailView* v) {
    furi_assert(v);
    return v->view;
}

void flock_detail_view_set_app(FlockDetailView* v, void* app) {
    with_view_model(v->view, FlockDetailModel * model, { model->app = app; }, false);
}

void flock_detail_view_set_callbacks(
    FlockDetailView* v,
    FlockDetailActionCallback mark_cb,
    FlockDetailActionCallback lock_cb,
    void* context) {
    v->mark_cb = mark_cb;
    v->lock_cb = lock_cb;
    v->ctx = context;
}

void flock_detail_view_reset(FlockDetailView* v) {
    with_view_model(v->view, FlockDetailModel * model, { model->top = 0; }, true);
}

void flock_detail_view_refresh(FlockDetailView* v) {
    with_view_model(v->view, FlockDetailModel * model, { UNUSED(model); }, true);
}
