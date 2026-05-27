/*
 * Custom scan list — see scan_canvas.h for the contract.
 *
 * Layout of the 128x64 monochrome screen:
 *
 *   y = 0..10   header bar:   "868.95 T1    47 meters"
 *   y = 11..51  list area:    4 rows × 10 px (3-px gap before the first)
 *   y = 53..63  footer bar:   "Top 3   Sort: Signal"
 *
 * Each row is drawn as:
 *   [4 vertical signal bars, 11 px]   [head text]   [...value at right]
 *
 * The currently selected row is rendered inverted (filled rectangle with
 * white-on-black text) so the cursor is unmistakable even when many rows
 * are visible. Up/Down scroll the cursor through the row list, OK fires
 * the tap callback with the original meter index, Back leaves the view
 * (handled by the scene manager via the standard navigation callback).
 */

#include "scan_canvas.h"
#include "../wmbus_app_i.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SCAN_VISIBLE_ROWS 4
#define SCAN_ROW_HEIGHT   10
#define SCAN_HEADER_H     11
#define SCAN_FOOTER_H     11
#define SCAN_LIST_TOP     (SCAN_HEADER_H + 1)
#define SCAN_LIST_BOTTOM  (64 - SCAN_FOOTER_H - 1)

typedef struct {
    ScanRow rows[WMBUS_MAX_METERS];
    size_t  row_count;
    size_t  cursor;             /* selected row in `rows[]`               */
    size_t  scroll;             /* index of the topmost visible row       */
    char    band[16];
    char    filter[14];
    char    sort[14];
    char    stats_tail[24];   /* receiver diagnostics, e.g. "Sy 42 Dec 38" */
    uint32_t total_telegrams;
} ScanModel;

struct ScanCanvas {
    View*           view;
    ScanCanvasTapCb cb;
    void*           cb_ctx;
};

/* ---------- Drawing helpers ---------- */

/* Draws a 4-bar signal indicator. Caller is responsible for the canvas
 * pen colour BEFORE and AFTER the call — we never reset it, so the rest
 * of the row continues painting in the same colour (matters for the
 * inverted/selected row). */
static void draw_signal_bars(Canvas* c, int x, int y, int8_t rssi) {
    int level = 0;
    if(rssi >= -55)      level = 4;
    else if(rssi >= -65) level = 3;
    else if(rssi >= -75) level = 2;
    else if(rssi >= -85) level = 1;
    for(int i = 0; i < 4; i++) {
        int h  = (i + 1) * 2;
        int bx = x + i * 3;
        int by = y - h;
        if(i < level) canvas_draw_box(c, bx, by, 2, h);
        else          canvas_draw_dot(c, bx, y - 1);
    }
}

static void draw_header(Canvas* c, const ScanModel* m) {
    canvas_set_font(c, FontSecondary);
    /* Inverted band/mode label on the left. */
    canvas_draw_box(c, 0, 0, 128, SCAN_HEADER_H);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str(c, 2, 8, m->band);
    char count[20];
    snprintf(count, sizeof(count), "%u/%lu",
             (unsigned)m->row_count, (unsigned long)m->total_telegrams);
    int w = canvas_string_width(c, count);
    canvas_draw_str(c, 128 - w - 2, 8, count);
    canvas_set_color(c, ColorBlack);
}

static void draw_footer(Canvas* c, const ScanModel* m) {
    /* Filter+sort left, optional stats trailer right-aligned. Drop sort
     * label first if the trailer would clip. */
    canvas_set_font(c, FontSecondary);
    int y0 = 64 - SCAN_FOOTER_H;
    canvas_draw_line(c, 0, y0, 127, y0);

    int tail_w = 0;
    if(m->stats_tail[0]) tail_w = canvas_string_width(c, m->stats_tail);

    char left[32];
    if(tail_w > 0 && tail_w > 60)
        snprintf(left, sizeof(left), "%s", m->filter);
    else
        snprintf(left, sizeof(left), "%s  %s", m->filter, m->sort);
    canvas_draw_str(c, 2, 64 - 2, left);

    if(m->stats_tail[0])
        canvas_draw_str(c, 128 - tail_w - 2, 64 - 2, m->stats_tail);
}

static void draw_row(Canvas* c, int y_top, const ScanRow* r, bool selected) {
    /* Selected rows paint inverted; the pen colour must remain set across
     * every draw call in the row. */
    if(selected) {
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(c, 0, y_top, 128, SCAN_ROW_HEIGHT);
        canvas_set_color(c, ColorWhite);   /* paint everything else white */
    } else {
        canvas_set_color(c, ColorBlack);
    }

    draw_signal_bars(c, 1, y_top + SCAN_ROW_HEIGHT - 1, r->rssi);

    canvas_set_font(c, FontSecondary);
    int text_x = 14;
    int text_y = y_top + SCAN_ROW_HEIGHT - 2;

    char head[28];
    strncpy(head, r->head, sizeof(head) - 1);
    head[sizeof(head) - 1] = 0;

    /* The caller fills `value` with the decoded reading or a status
     * badge (ENC / BAD / DEC). The head ("MFR ID") is the only stable
     * identifier we have, so we never let the value shrink it below a
     * minimum width — instead the value gets truncated from its right
     * edge until both fit. Without this clamp a verbose value (e.g. a
     * raw hex dump from a proprietary frame) used to wipe the head off
     * the row entirely, leaving the user staring at nothing but hex. */
    const int kHeadMinW = 60;     /* enough for "MFR 12345678" + medium */
    char val[24];
    val[0] = 0;
    if(r->value[0]) {
        strncpy(val, r->value, sizeof(val) - 1);
        val[sizeof(val) - 1] = 0;
    }
    int vw = val[0] ? canvas_string_width(c, val) : 0;
    int max_val_w = 128 - text_x - kHeadMinW - 4;
    while(vw > max_val_w && val[0]) {
        val[strlen(val) - 1] = 0;
        vw = canvas_string_width(c, val);
    }

    int max_head_w = 128 - text_x - vw - 4;
    while(canvas_string_width(c, head) > max_head_w && head[0]) {
        head[strlen(head) - 1] = 0;
    }

    canvas_draw_str(c, text_x, text_y, head);
    if(val[0]) canvas_draw_str(c, 128 - vw - 2, text_y, val);

    canvas_set_color(c, ColorBlack);
}

/* ---------- View callbacks ---------- */

static void scan_view_draw(Canvas* c, void* m_) {
    const ScanModel* m = m_;
    canvas_clear(c);
    draw_header(c, m);

    if(m->row_count == 0) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter,
                                "Listening...");
        draw_footer(c, m);
        return;
    }

    /* Visible window: rows[scroll .. scroll+SCAN_VISIBLE_ROWS-1]. */
    for(size_t i = 0; i < SCAN_VISIBLE_ROWS; i++) {
        size_t idx = m->scroll + i;
        if(idx >= m->row_count) break;
        int y = SCAN_LIST_TOP + (int)i * SCAN_ROW_HEIGHT;
        draw_row(c, y, &m->rows[idx], idx == m->cursor);
    }

    /* Scrollbar — thin column on the right edge of the list area. */
    if(m->row_count > SCAN_VISIBLE_ROWS) {
        elements_scrollbar_pos(c, 127, SCAN_LIST_TOP, SCAN_LIST_BOTTOM - SCAN_LIST_TOP,
                               m->cursor, m->row_count);
    }

    draw_footer(c, m);
}

static bool scan_view_input(InputEvent* ev, void* ctx) {
    ScanCanvas* sc = ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    bool handled = false;
    bool need_redraw = false;

    if(ev->key == InputKeyUp || ev->key == InputKeyDown) {
        with_view_model(sc->view, ScanModel * m,
            {
                if(m->row_count == 0) {
                    /* nothing */
                } else if(ev->key == InputKeyUp) {
                    if(m->cursor > 0) m->cursor--;
                    if(m->cursor < m->scroll) m->scroll = m->cursor;
                    need_redraw = true;
                } else {
                    if(m->cursor + 1 < m->row_count) m->cursor++;
                    if(m->cursor >= m->scroll + SCAN_VISIBLE_ROWS)
                        m->scroll = m->cursor - SCAN_VISIBLE_ROWS + 1;
                    need_redraw = true;
                }
            },
            need_redraw);
        handled = true;
    } else if(ev->key == InputKeyOk && ev->type == InputTypeShort) {
        uint16_t mi = 0xFFFF;
        with_view_model(sc->view, ScanModel * m,
            {
                if(m->cursor < m->row_count) mi = m->rows[m->cursor].meter_idx;
            },
            false);
        if(mi != 0xFFFF && sc->cb) sc->cb(sc->cb_ctx, mi);
        handled = true;
    }
    return handled;
}

/* ---------- Public API ---------- */

ScanCanvas* scan_canvas_alloc(void) {
    ScanCanvas* sc = malloc(sizeof(ScanCanvas));
    memset(sc, 0, sizeof(*sc));
    sc->view = view_alloc();
    view_allocate_model(sc->view, ViewModelTypeLocking, sizeof(ScanModel));
    view_set_context(sc->view, sc);
    view_set_draw_callback(sc->view, scan_view_draw);
    view_set_input_callback(sc->view, scan_view_input);
    return sc;
}

void scan_canvas_free(ScanCanvas* sc) {
    if(!sc) return;
    view_free(sc->view);
    free(sc);
}

View* scan_canvas_get_view(ScanCanvas* sc) { return sc->view; }

void scan_canvas_set_tap_callback(ScanCanvas* sc, ScanCanvasTapCb cb, void* ctx) {
    sc->cb = cb;
    sc->cb_ctx = ctx;
}

void scan_canvas_set_rows(ScanCanvas* sc, const ScanRow* rows, size_t n) {
    if(n > WMBUS_MAX_METERS) n = WMBUS_MAX_METERS;
    with_view_model(sc->view, ScanModel * m,
        {
            memcpy(m->rows, rows, n * sizeof(ScanRow));
            m->row_count = n;
            if(m->cursor >= n) m->cursor = n ? n - 1 : 0;
            if(m->scroll > m->cursor) m->scroll = m->cursor;
            if(m->cursor >= m->scroll + SCAN_VISIBLE_ROWS)
                m->scroll = m->cursor - SCAN_VISIBLE_ROWS + 1;
        },
        true);
}

void scan_canvas_set_meta(ScanCanvas* sc, const char* band,
                          const char* filter, const char* sort,
                          uint32_t total_telegrams,
                          const char* stats_tail) {
    with_view_model(sc->view, ScanModel * m,
        {
            if(band)   { strncpy(m->band, band,     sizeof(m->band)   - 1); m->band[sizeof(m->band) - 1]     = 0; }
            if(filter) { strncpy(m->filter, filter, sizeof(m->filter) - 1); m->filter[sizeof(m->filter) - 1] = 0; }
            if(sort)   { strncpy(m->sort, sort,     sizeof(m->sort)   - 1); m->sort[sizeof(m->sort) - 1]     = 0; }
            if(stats_tail) {
                strncpy(m->stats_tail, stats_tail, sizeof(m->stats_tail) - 1);
                m->stats_tail[sizeof(m->stats_tail) - 1] = 0;
            } else {
                m->stats_tail[0] = 0;
            }
            m->total_telegrams = total_telegrams;
        },
        true);
}

void scan_canvas_focus_meter(ScanCanvas* sc, uint16_t meter_idx) {
    with_view_model(sc->view, ScanModel * m,
        {
            for(size_t i = 0; i < m->row_count; i++) {
                if(m->rows[i].meter_idx == meter_idx) {
                    m->cursor = i;
                    if(m->cursor < m->scroll) m->scroll = m->cursor;
                    if(m->cursor >= m->scroll + SCAN_VISIBLE_ROWS)
                        m->scroll = m->cursor - SCAN_VISIBLE_ROWS + 1;
                    break;
                }
            }
        },
        true);
}
