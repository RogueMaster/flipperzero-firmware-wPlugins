/* Detail canvas — pixel-perfect rendering of one meter's decoded fields.
 *
 *   y =  0..17   inverted header bar: id (bold) + RSSI, subtitle + badge
 *                + packet count
 *   y = 18..63   list area, ~5 rows of 9 px each
 *
 * Per row the label is drawn in FontPrimary (bold) on the left, the value
 * in FontSecondary on the right. If a value is wider than the value
 * column we truncate with a trailing ellipsis instead of letting it
 * overlap the label like the older renderer did. When the parser falls
 * back on a narrative line that has no clean label/value split (`label`
 * empty), the line is rendered full-width so banner-style messages still
 * look intentional.
 *
 * Pressing OK toggles into a raw-bytes view that hex-dumps the most
 * recent APDU 16 bytes per row — handy for spotting unknown meters in
 * the field. Up/Down scroll the active view; Back leaves the screen via
 * the standard navigation callback. */

#include "detail_canvas.h"
#include "../wmbus_app_i.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DETAIL_HEADER_H   18
#define DETAIL_ROW_H      9
#define DETAIL_LIST_TOP   (DETAIL_HEADER_H + 1)
#define DETAIL_VISIBLE    ((64 - DETAIL_LIST_TOP) / DETAIL_ROW_H)
#define DETAIL_MAX_ROWS   16
#define DETAIL_RAW_BYTES_PER_ROW 8
#define DETAIL_RAW_MAX_BYTES     256

typedef struct {
    char     id_line[24];
    char     subtitle[28];
    int8_t   rssi;
    uint32_t telegram_count;
    bool     encrypted;
    char     badge[5];           /* "ENC", "DEC", "BAD" or empty           */
    DetailRow rows[DETAIL_MAX_ROWS];
    size_t   row_count;
    size_t   scroll;
    /* Raw mode: cached APDU + cursor for OK-toggled hex dump. */
    uint8_t  apdu[DETAIL_RAW_MAX_BYTES];
    size_t   apdu_len;
    bool     raw_view;
    size_t   raw_scroll;         /* row index into the dump grid             */
} DetailModel;

struct DetailCanvas {
    View* view;
};

/* ---------- Drawing ---------- */

static void draw_header(Canvas* c, const DetailModel* m) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, 128, DETAIL_HEADER_H);
    canvas_set_color(c, ColorWhite);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 9, m->id_line);

    /* RSSI on the right of the first line. */
    char rssi[10];
    snprintf(rssi, sizeof(rssi), "%ddBm", (int)m->rssi);
    canvas_set_font(c, FontSecondary);
    int rw = canvas_string_width(c, rssi);
    canvas_draw_str(c, 128 - rw - 2, 9, rssi);

    /* Second line: subtitle + optional badge + packet count. */
    char tail[24];
    if(m->badge[0])
        snprintf(tail, sizeof(tail), "%s %lu pkt",
                 m->badge, (unsigned long)m->telegram_count);
    else
        snprintf(tail, sizeof(tail), "%lu pkt",
                 (unsigned long)m->telegram_count);
    int tw = canvas_string_width(c, tail);

    /* Truncate subtitle to fit alongside the tail. */
    char sub[28];
    strncpy(sub, m->subtitle, sizeof(sub) - 1); sub[sizeof(sub)-1] = 0;
    int max_sub_w = 128 - 4 - tw - 2;
    while(canvas_string_width(c, sub) > max_sub_w && sub[0]) sub[strlen(sub) - 1] = 0;

    canvas_draw_str(c, 2, 17, sub);
    canvas_draw_str(c, 128 - tw - 2, 17, tail);

    canvas_set_color(c, ColorBlack);
}

/* Truncate `s` (in place) to at most `max_w` pixels in the current font,
 * appending a single-character ellipsis (".") when shortened. The Flipper
 * fonts don't include U+2026, hence the ASCII fallback. */
static void fit_with_ellipsis(Canvas* c, char* s, int max_w) {
    if(canvas_string_width(c, s) <= max_w) return;
    size_t n = strlen(s);
    while(n > 1) {
        s[n - 1] = '.';
        s[n] = 0;
        if(canvas_string_width(c, s) <= max_w) return;
        n--;
    }
}

static void draw_rows(Canvas* c, const DetailModel* m) {
    canvas_set_color(c, ColorBlack);

    if(m->row_count == 0) {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignCenter,
                                "(no data yet)");
        return;
    }

    size_t end = m->scroll + DETAIL_VISIBLE;
    if(end > m->row_count) end = m->row_count;

    bool has_scrollbar = m->row_count > DETAIL_VISIBLE;
    int  right_edge   = has_scrollbar ? 122 : 126;

    for(size_t i = m->scroll; i < end; i++) {
        int y = DETAIL_LIST_TOP + (int)(i - m->scroll) * DETAIL_ROW_H + DETAIL_ROW_H - 2;
        const DetailRow* r = &m->rows[i];

        /* Narrative row: when the parser yielded an empty label, the
         * source line was a single-column note. Render it full width in
         * the secondary font so it doesn't pretend to be a label/value
         * pair. */
        if(r->label[0] == 0) {
            canvas_set_font(c, FontSecondary);
            char tmp[32]; strncpy(tmp, r->value, sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = 0;
            fit_with_ellipsis(c, tmp, right_edge - 2);
            canvas_draw_str(c, 2, y, tmp);
            continue;
        }

        /* Label in bold, left-aligned. Cap at 56 px so values always
         * have a clean column to the right. */
        canvas_set_font(c, FontPrimary);
        char lbl[12];
        strncpy(lbl, r->label, sizeof(lbl) - 1); lbl[sizeof(lbl)-1] = 0;
        fit_with_ellipsis(c, lbl, 56);
        canvas_draw_str(c, 2, y, lbl);

        /* Value in secondary font, right-aligned, truncated to fit the
         * remaining gutter so it can never bleed into the label. */
        canvas_set_font(c, FontSecondary);
        char val[24];
        strncpy(val, r->value, sizeof(val) - 1); val[sizeof(val)-1] = 0;
        int label_w = canvas_string_width(c, lbl);
        int max_val_w = right_edge - (2 + label_w + 4);
        if(max_val_w < 8) max_val_w = 8;
        fit_with_ellipsis(c, val, max_val_w);
        int vw = canvas_string_width(c, val);
        canvas_draw_str(c, right_edge - vw, y, val);
    }

    if(has_scrollbar) {
        elements_scrollbar_pos(
            c, 127, DETAIL_LIST_TOP, 64 - DETAIL_LIST_TOP,
            m->scroll, m->row_count - DETAIL_VISIBLE + 1);
    }
}

/* Hex dump rendering — DETAIL_RAW_BYTES_PER_ROW bytes per row, row index
 * shown in the leftmost column so a developer can correlate with a
 * decoder spec. */
static void draw_raw(Canvas* c, const DetailModel* m) {
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    size_t total_rows = (m->apdu_len + DETAIL_RAW_BYTES_PER_ROW - 1) / DETAIL_RAW_BYTES_PER_ROW;
    if(total_rows == 0) {
        canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignCenter,
                                "(no payload)");
        return;
    }
    bool has_scrollbar = total_rows > DETAIL_VISIBLE;
    size_t end = m->raw_scroll + DETAIL_VISIBLE;
    if(end > total_rows) end = total_rows;

    for(size_t i = m->raw_scroll; i < end; i++) {
        int y = DETAIL_LIST_TOP + (int)(i - m->raw_scroll) * DETAIL_ROW_H + DETAIL_ROW_H - 2;
        size_t off = i * DETAIL_RAW_BYTES_PER_ROW;
        char line[40];
        int  pos = snprintf(line, sizeof(line), "%02X ", (unsigned)off);
        for(size_t k = 0; k < DETAIL_RAW_BYTES_PER_ROW && off + k < m->apdu_len; k++) {
            pos += snprintf(line + pos, sizeof(line) - pos,
                            "%02X", m->apdu[off + k]);
        }
        canvas_draw_str(c, 2, y, line);
    }

    if(has_scrollbar) {
        elements_scrollbar_pos(
            c, 127, DETAIL_LIST_TOP, 64 - DETAIL_LIST_TOP,
            m->raw_scroll, total_rows - DETAIL_VISIBLE + 1);
    }
}

/* Tiny chevron + "OK" hint stamped at the bottom-right when a raw view
 * is available. Drawn over the scroll-bar gutter so we don't steal a
 * row from the actual content. The "OK" label tells the user how to
 * cycle, the arrow direction shows which mode they'd switch to. */
static void draw_ok_hint(Canvas* c, const DetailModel* m) {
    if(m->apdu_len == 0) return;
    canvas_set_font(c, FontSecondary);
    const char* hint = m->raw_view ? "OK fields" : "OK raw";
    int w = canvas_string_width(c, hint);
    /* Background patch so the hint stays readable above content. */
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 128 - w - 4, 64 - 9, w + 4, 9);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 128 - w - 4, 64 - 9, w + 4, 9);
    canvas_draw_str(c, 128 - w - 2, 64 - 2, hint);
}

static void detail_view_draw(Canvas* c, void* m_) {
    const DetailModel* m = m_;
    canvas_clear(c);
    draw_header(c, m);
    if(m->raw_view) draw_raw(c, m);
    else            draw_rows(c, m);
    draw_ok_hint(c, m);
}

static bool detail_view_input(InputEvent* ev, void* ctx) {
    DetailCanvas* dc = ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    if(ev->key != InputKeyUp && ev->key != InputKeyDown && ev->key != InputKeyOk)
        return false;
    bool need = false;
    with_view_model(dc->view, DetailModel * m,
        {
            if(ev->key == InputKeyOk) {
                /* Toggle decoded ↔ raw. Only meaningful when we actually
                 * have an APDU to show — otherwise stay in fields mode. */
                if(m->apdu_len > 0) {
                    m->raw_view = !m->raw_view;
                    need = true;
                }
            } else if(m->raw_view) {
                size_t total = (m->apdu_len + DETAIL_RAW_BYTES_PER_ROW - 1) / DETAIL_RAW_BYTES_PER_ROW;
                if(total > DETAIL_VISIBLE) {
                    if(ev->key == InputKeyUp && m->raw_scroll > 0) {
                        m->raw_scroll--; need = true;
                    } else if(ev->key == InputKeyDown &&
                              m->raw_scroll + DETAIL_VISIBLE < total) {
                        m->raw_scroll++; need = true;
                    }
                }
            } else if(m->row_count > DETAIL_VISIBLE) {
                if(ev->key == InputKeyUp && m->scroll > 0) { m->scroll--; need = true; }
                else if(ev->key == InputKeyDown &&
                        m->scroll + DETAIL_VISIBLE < m->row_count) { m->scroll++; need = true; }
            }
        },
        need);
    return true;
}

/* ---------- Public API ---------- */

DetailCanvas* detail_canvas_alloc(void) {
    DetailCanvas* dc = malloc(sizeof(DetailCanvas));
    memset(dc, 0, sizeof(*dc));
    dc->view = view_alloc();
    view_allocate_model(dc->view, ViewModelTypeLocking, sizeof(DetailModel));
    view_set_context(dc->view, dc);
    view_set_draw_callback(dc->view, detail_view_draw);
    view_set_input_callback(dc->view, detail_view_input);
    return dc;
}

void detail_canvas_free(DetailCanvas* dc) {
    if(!dc) return;
    view_free(dc->view);
    free(dc);
}

View* detail_canvas_get_view(DetailCanvas* dc) { return dc->view; }

void detail_canvas_set_header(DetailCanvas* dc,
                              const char* line1, const char* line2,
                              int8_t rssi, uint32_t pkt, bool encrypted,
                              const char* badge) {
    with_view_model(dc->view, DetailModel * m,
        {
            strncpy(m->id_line,  line1 ? line1 : "", sizeof(m->id_line)  - 1);
            strncpy(m->subtitle, line2 ? line2 : "", sizeof(m->subtitle) - 1);
            m->id_line[sizeof(m->id_line) - 1] = 0;
            m->subtitle[sizeof(m->subtitle) - 1] = 0;
            m->rssi = rssi;
            m->telegram_count = pkt;
            m->encrypted = encrypted;
            if(badge) {
                strncpy(m->badge, badge, sizeof(m->badge) - 1);
                m->badge[sizeof(m->badge) - 1] = 0;
            } else {
                m->badge[0] = 0;
            }
        },
        true);
}

void detail_canvas_set_rows(DetailCanvas* dc, const DetailRow* rows, size_t n) {
    if(n > DETAIL_MAX_ROWS) n = DETAIL_MAX_ROWS;
    with_view_model(dc->view, DetailModel * m,
        {
            for(size_t i = 0; i < n; i++) m->rows[i] = rows[i];
            m->row_count = n;
            if(m->scroll > n) m->scroll = 0;
            /* Always reset to decoded view when fields change so the user
             * sees the new content first; OK still toggles on demand. */
            m->raw_view = false;
        },
        true);
}

void detail_canvas_set_raw(DetailCanvas* dc,
                           const uint8_t* apdu, size_t apdu_len) {
    with_view_model(dc->view, DetailModel * m,
        {
            size_t cap = sizeof(m->apdu);
            size_t n   = (apdu && apdu_len) ? apdu_len : 0;
            if(n > cap) n = cap;
            if(n) memcpy(m->apdu, apdu, n);
            m->apdu_len   = n;
            m->raw_scroll = 0;
            /* Switching meters resets the toggle so the user lands on the
             * decoded view first. */
            m->raw_view = false;
        },
        true);
}
