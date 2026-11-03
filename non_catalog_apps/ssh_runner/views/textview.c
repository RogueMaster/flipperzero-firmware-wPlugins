/* Word-wrapped text view (see textview.h). SPDX-License-Identifier: GPL-3.0-only
 *
 * RAM: a FAP leaves little heap, and Output can hold ~4 KB of short lines. So the view
 * keeps one copy of each text block in a buffer that is reused (no churn), or none at all
 * (Output: text_view_set_ref), and only a sparse index: the start offset of every TV_ANCHOR_EVERY-th line. Drawing re-wraps the
 * handful of visible lines from the nearest anchor (core/wrap.c is resumable). */
#include "textview.h"

#include <furi.h>
#include <gui/elements.h>

#include "../core/wrap.h"
#include "../core/textproc.h"
#include "tofu.h"

#define TV_W            128
#define TV_H            64
#define TV_MARGIN       2
#define TV_SCROLL_W     3 /* stock scrollbar width incl. its gap */
/* Every glyph's ink keeps 2 px from all four edges (tallest FontPrimary glyphs reach 9
 * rows above the baseline, descenders 2 rows down incl. the baseline row). */
#define TV_HEADER_Y     3 /* AlignTop: baseline 11, ink rows 2..12 */
#define TV_HEADER_H     14
#define TV_HEADER_AIR   3 /* more room under the header, taken only from spare rows */
#define TV_BUTTONS_H    12 /* stock button bar */
#define TV_FOOTER_H     14 /* rule at row 50, text baseline 60 */
#define TV_FOOTER_BASE  60
#define TV_LINE_MAX     96 /* bytes of one drawn line */
#define TV_FIRST_CHAR   32
#define TV_GLYPHS       95
#define TV_ANCHOR_EVERY 16
#define TV_MAX_VISIBLE  8
#define TV_BLOCKS       TEXT_VIEW_BLOCKS

typedef struct {
    char* buf; /* reused; grows, never shrinks while the app runs */
    const char* ref; /* the caller's text instead of buf (text_view_set_ref) */
    size_t cap;
    size_t len;
    uint8_t font;
    bool used;
    uint16_t count; /* lines */
    uint16_t* anchors; /* offset of line k*TV_ANCHOR_EVERY */
    uint16_t anchors_cap;
} TvBlock;

typedef struct {
    char header[48];
    bool header_mono; /* FontKeyboard: an address, command, path or name */
    const Icon* icon;
    TvBlock block[TV_BLOCKS];
    char footer[48];
    const Icon* footer_icon;
    char hint[12]; /* the footer's right end: a key's sign and its action ("Stop") */
    const Icon* hint_icon;
    char btn[3][16];
    bool follow_end;
    uint32_t version;

    uint32_t laid_version;
    uint8_t laid_width;
    uint16_t nlines;
    uint16_t scroll;
    uint8_t visible; /* lines that fit, from the last draw */
    bool at_end;

    uint8_t gw[2][TV_GLYPHS];
    bool gw_ok[2];
} TvModel;

struct TextView {
    View* view;
    TextViewCallback cb;
    void* cb_ctx;
    TvModel* locked; /* between text_view_lock() and _unlock() */
};

static Font tv_font(uint8_t f) {
    return f == TextViewFontMono ? FontKeyboard : FontSecondary;
}

static uint8_t gw_lookup(void* ctx, char c) {
    const uint8_t* table = ctx;
    uint8_t u = (uint8_t)c;
    if(u < TV_FIRST_CHAR || u >= TV_FIRST_CHAR + TV_GLYPHS) u = '?';
    return table[u - TV_FIRST_CHAR];
}

static void tv_fill_glyphs(Canvas* canvas, TvModel* m, uint8_t f) {
    if(m->gw_ok[f]) return;
    canvas_set_font(canvas, tv_font(f));
    for(uint8_t i = 0; i < TV_GLYPHS; i++)
        m->gw[f][i] = (uint8_t)canvas_glyph_width(canvas, (uint16_t)(TV_FIRST_CHAR + i));
    m->gw_ok[f] = true;
}

static const char* tv_text(const TvBlock* b) {
    return b->ref ? b->ref : b->buf ? b->buf : "";
}

static void anchor_push(TvBlock* b, uint16_t idx, size_t off) {
    if(idx >= b->anchors_cap) {
        uint16_t cap = b->anchors_cap ? b->anchors_cap * 2 : 64; /* few, big steps */
        while(cap <= idx)
            cap *= 2;
        b->anchors = realloc(b->anchors, cap * sizeof(uint16_t));
        b->anchors_cap = cap;
    }
    b->anchors[idx] = (uint16_t)off;
}

static void tv_layout(Canvas* canvas, TvModel* m, uint8_t width) {
    if(m->laid_version == m->version && m->laid_width == width) return;
    bool was_end = m->at_end || m->nlines == 0;
    m->nlines = 0;
    for(uint8_t bi = 0; bi < TV_BLOCKS; bi++) {
        TvBlock* b = &m->block[bi];
        b->count = 0;
        if(!b->used) continue;
        tv_fill_glyphs(canvas, m, b->font);
        const char* t = tv_text(b);
        size_t len = b->len;
        size_t pos = 0;
        uint16_t a = 0;
        while(true) {
            anchor_push(b, a++, pos);
            size_t next = 0;
            size_t got = wrap_next(
                t + pos, len - pos, width, gw_lookup, m->gw[b->font], NULL, TV_ANCHOR_EVERY, &next);
            b->count += (uint16_t)got;
            if(got < TV_ANCHOR_EVERY || pos + next >= len || b->count > 0xFF00) break;
            pos += next;
        }
        m->nlines += b->count;
    }
    m->laid_version = m->version;
    m->laid_width = width;
    if(m->follow_end && was_end) m->scroll = 0xFFFF; /* clamped in draw */
}

/* Line `idx` of the whole body: which block, and its text. */
static bool
    tv_line(TvModel* m, uint16_t idx, uint8_t width, uint8_t* block, char* buf, size_t size) {
    uint8_t bi = 0;
    while(bi + 1 < TV_BLOCKS && idx >= m->block[bi].count) {
        idx -= m->block[bi].count;
        bi++;
    }
    TvBlock* b = &m->block[bi];
    if(!b->used || idx >= b->count) return false;
    const char* t = tv_text(b);
    size_t len = b->len;
    size_t start = b->anchors[idx / TV_ANCHOR_EVERY];
    WrapLine lines[TV_ANCHOR_EVERY];
    size_t k = idx % TV_ANCHOR_EVERY;
    size_t got =
        wrap_next(t + start, len - start, width, gw_lookup, m->gw[b->font], lines, k + 1, NULL);
    if(got <= k) return false;
    size_t n = lines[k].len < size - 1 ? lines[k].len : size - 1;
    memcpy(buf, t + start + lines[k].off, n);
    buf[n] = 0;
    for(size_t i = 0; i < n; i++)
        if(buf[i] == '\r' || buf[i] == '\t') buf[i] = ' ';
    *block = bi;
    return true;
}

/* Stock-looking scrollbar (dotted track, 3 px thumb) with a proportional thumb: its size is
 * the visible share, its position the scrolled share (stock elements_scrollbar_pos sizes
 * it per scroll step, which looked wrong on long texts). */
static void tv_scrollbar(
    Canvas* canvas,
    uint8_t y,
    uint8_t h,
    uint16_t scroll,
    uint8_t visible,
    uint16_t nlines) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, TV_W - 3, y, 3, h);
    canvas_set_color(canvas, ColorBlack);
    for(int i = y; i < y + h; i++)
        if(i % 2 == 0) canvas_draw_dot(canvas, TV_W - 2, i);
    uint16_t th = (uint16_t)((uint32_t)h * visible / nlines);
    if(th < 3) th = 3;
    if(th > h) th = h;
    uint16_t max_scroll = nlines - visible;
    uint16_t ty = max_scroll ? (uint16_t)((uint32_t)(h - th) * scroll / max_scroll) : 0;
    canvas_draw_box(canvas, TV_W - 3, y + ty, 3, th);
}

/* Fit one line into `width` with the stock "...", never leaving a space before the dots
 * ("$ sudo systemctl..." rather than "$ sudo systemctl ..."). */
static void tv_fit(Canvas* canvas, FuriString* s, uint8_t width) {
    if(canvas_string_width(canvas, furi_string_get_cstr(s)) <= width) return;
    uint16_t dots = canvas_string_width(canvas, "...");
    size_t n = furi_string_size(s);
    while(n && (furi_string_get_cstr(s)[n - 1] == ' ' ||
                canvas_string_width(canvas, furi_string_get_cstr(s)) + dots > width)) {
        furi_string_left(s, --n);
    }
    furi_string_cat(s, "...");
}

static void tv_draw(Canvas* canvas, void* model) {
    TvModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    uint8_t top = TV_MARGIN;
    if(m->header[0]) {
        /* AlignTop at row 3: FontPrimary caps on rows 3..10, FontKeyboard caps on 4..10
         * (its '/' and brackets from row 3), both with descenders down to row 12 */
        canvas_set_font(canvas, m->header_mono ? FontKeyboard : FontPrimary);
        uint8_t hx = 0, hw = TV_W;
        if(m->icon) {
            /* the 8-row warning sign on the caps (rows 3..10, FontKeyboard 3..9) */
            canvas_draw_icon(canvas, TV_MARGIN, 3, m->icon);
            hx = 16;
            hw = TV_W - 2 * 16;
        }
        FuriString* h = furi_string_alloc_set(m->header);
        tv_fit(canvas, h, hw - 2 * TV_MARGIN);
        canvas_draw_str_aligned(
            canvas,
            m->icon ? hx + hw / 2 : TV_W / 2,
            TV_HEADER_Y,
            AlignCenter,
            AlignTop,
            furi_string_get_cstr(h));
        furi_string_free(h);
        top = TV_HEADER_H;
    }
    bool buttons = m->btn[0][0] || m->btn[1][0] || m->btn[2][0];
    /* the area the body may ink: above the button bar, the footer rule, or the 2 px margin */
    uint8_t bottom = buttons      ? TV_H - TV_BUTTONS_H :
                     m->footer[0] ? TV_H - TV_FOOTER_H :
                                    TV_H - TV_MARGIN;

    uint8_t width = TV_W - 2 * TV_MARGIN - TV_SCROLL_W;
    tv_layout(canvas, m, width);

    /* one line pitch for the whole body: the tallest font in use; the last line keeps its
     * descenders (g, p, y, brackets) above `bottom` */
    uint8_t step = 0, ascent = 0, descent = 0;
    for(uint8_t bi = 0; bi < TV_BLOCKS; bi++) {
        if(!m->block[bi].used && bi) continue;
        const CanvasFontParameters* fp =
            canvas_get_font_params(canvas, tv_font(m->block[bi].font));
        if(fp->leading_default > step) step = fp->leading_default;
        if(fp->height > ascent) ascent = fp->height;
        if(fp->descender > descent) descent = fp->descender;
    }
    uint8_t avail = bottom > top ? bottom - top : 0;
    uint8_t visible =
        avail >= ascent + descent ? (uint8_t)(1 + (avail - ascent - descent) / step) : 0;
    if(visible > TV_MAX_VISIBLE) visible = TV_MAX_VISIBLE;
    m->visible = visible;
    /* rows left below the last line go under the header first, so the title does not sit
     * on the body; never at the cost of a line */
    if(m->header[0] && visible) {
        uint8_t used = ascent + descent + (visible - 1) * step;
        uint8_t spare = avail > used ? avail - used : 0;
        top += spare < TV_HEADER_AIR ? spare : TV_HEADER_AIR;
    }
    uint16_t max_scroll = m->nlines > visible ? m->nlines - visible : 0;
    if(m->scroll > max_scroll) m->scroll = max_scroll;
    m->at_end = m->scroll >= max_scroll;

    char buf[TV_LINE_MAX];
    for(uint8_t i = 0; i < visible && m->scroll + i < m->nlines; i++) {
        uint8_t bi = 0;
        if(!tv_line(m, m->scroll + i, width, &bi, buf, sizeof(buf))) break;
        canvas_set_font(canvas, tv_font(m->block[bi].font));
        if(strchr(buf, TEXT_TOFU))
            tofu_draw_str(
                canvas, tv_font(m->block[bi].font), TV_MARGIN, top + ascent + i * step, buf);
        else
            canvas_draw_str(canvas, TV_MARGIN, top + ascent + i * step, buf);
    }
    /* the track stops 2 px above the footer rule / button bar so the thumb never merges */
    if(m->nlines > visible) {
        uint8_t track_end = buttons || m->footer[0] ? bottom - 2 : TV_H;
        tv_scrollbar(canvas, top, track_end - top, m->scroll, visible, m->nlines);
    }

    if(buttons) {
        canvas_set_font(canvas, FontSecondary); /* stock button font, whatever the body used */
        if(m->btn[0][0]) elements_button_left(canvas, m->btn[0]);
        if(m->btn[1][0]) elements_button_center(canvas, m->btn[1]);
        if(m->btn[2][0]) elements_button_right(canvas, m->btn[2]);
    } else if(m->footer[0]) {
        canvas_draw_line(canvas, 0, bottom, TV_W - 1, bottom);
        canvas_set_font(canvas, FontSecondary);
        uint8_t fx = TV_MARGIN, fr = TV_W - TV_MARGIN;
        if(m->hint[0]) {
            fr -= canvas_string_width(canvas, m->hint);
            canvas_draw_str(canvas, fr, TV_FOOTER_BASE, m->hint);
            if(m->hint_icon) {
                fr -= icon_get_width(m->hint_icon) + 2;
                canvas_draw_icon(
                    canvas, fr, TV_FOOTER_BASE + 1 - icon_get_height(m->hint_icon), m->hint_icon);
            }
            fr -= 6;
        }
        if(m->footer_icon) {
            /* bottom row on the baseline: an 8-row sign on the caps, the 9-row busy sign likewise */
            canvas_draw_icon(
                canvas,
                TV_MARGIN,
                TV_FOOTER_BASE + 1 - icon_get_height(m->footer_icon),
                m->footer_icon);
            fx += icon_get_width(m->footer_icon) + 3;
        }
        FuriString* f = furi_string_alloc_set(m->footer);
        tv_fit(canvas, f, fr - fx);
        canvas_draw_str(canvas, fx, TV_FOOTER_BASE, furi_string_get_cstr(f));
        furi_string_free(f);
    }
}

static bool tv_input(InputEvent* event, void* context) {
    TextView* tv = context;
    if(event->key == InputKeyBack) return false;
    bool scroll = event->key == InputKeyUp || event->key == InputKeyDown;
    if(scroll && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(
            tv->view,
            TvModel * m,
            {
                if(event->key == InputKeyUp && m->scroll > 0) m->scroll--;
                if(event->key == InputKeyDown && m->scroll + m->visible < m->nlines) m->scroll++;
            },
            true);
        return true;
    }
    if(!scroll && event->type == InputTypeShort && tv->cb) {
        tv->cb(tv->cb_ctx, event->key);
        return true;
    }
    return scroll;
}

TextView* text_view_alloc(void) {
    TextView* tv = malloc(sizeof(TextView));
    memset(tv, 0, sizeof(*tv));
    tv->view = view_alloc();
    view_allocate_model(tv->view, ViewModelTypeLocking, sizeof(TvModel));
    view_set_context(tv->view, tv);
    view_set_draw_callback(tv->view, tv_draw);
    view_set_input_callback(tv->view, tv_input);
    with_view_model(
        tv->view,
        TvModel * m,
        {
            memset(m, 0, sizeof(*m));
            m->version = 1;
        },
        false);
    return tv;
}

/* Back to empty; keeps the buffers' capacity and the glyph tables. */
static void tv_clear(TvModel* m) {
    for(uint8_t b = 0; b < TV_BLOCKS; b++) {
        m->block[b].ref = NULL;
        m->block[b].len = 0;
        m->block[b].used = false;
        m->block[b].count = 0;
    }
    m->header[0] = 0;
    m->header_mono = false;
    m->icon = NULL;
    m->footer[0] = 0;
    m->footer_icon = NULL;
    m->hint[0] = 0;
    m->hint_icon = NULL;
    memset(m->btn, 0, sizeof(m->btn));
    m->follow_end = false;
    m->nlines = 0;
    m->scroll = 0;
    m->at_end = false;
    m->version++;
}

void text_view_free(TextView* tv) {
    with_view_model(
        tv->view,
        TvModel * m,
        {
            for(uint8_t b = 0; b < TV_BLOCKS; b++) {
                free(m->block[b].buf);
                free(m->block[b].anchors);
            }
        },
        false);
    view_free(tv->view);
    free(tv);
}

View* text_view_get_view(TextView* tv) {
    return tv->view;
}

void text_view_reset(TextView* tv) {
    tv->cb = NULL;
    tv->cb_ctx = NULL;
    with_view_model(tv->view, TvModel * m, { tv_clear(m); }, true);
}

static void tv_set_header(TextView* tv, const char* header, const Icon* icon, bool mono) {
    with_view_model(
        tv->view,
        TvModel * m,
        {
            snprintf(m->header, sizeof(m->header), "%s", header ? header : "");
            m->header_mono = mono;
            m->icon = icon;
        },
        true);
}

void text_view_set_header(TextView* tv, const char* header, const Icon* icon) {
    tv_set_header(tv, header, icon, text_is_technical(header));
}

void text_view_set_header_mono(TextView* tv, const char* header, const Icon* icon) {
    tv_set_header(tv, header, icon, true);
}

static bool tv_reserve(TvBlock* b, size_t cap) {
    if(cap <= b->cap) return true;
    char* p = realloc(b->buf, cap);
    if(!p) return false;
    b->buf = p;
    b->cap = cap;
    return true;
}

void text_view_set_text(TextView* tv, uint8_t block, const char* text, TextViewFont font) {
    if(block >= TV_BLOCKS) return;
    size_t len = text ? strlen(text) : 0;
    with_view_model(
        tv->view,
        TvModel * m,
        {
            TvBlock* b = &m->block[block];
            b->ref = NULL;
            if(text && tv_reserve(b, len + 1)) {
                memcpy(b->buf, text, len + 1);
                b->len = len;
                b->used = true;
            } else {
                b->len = 0;
                b->used = false;
            }
            b->font = (uint8_t)font;
            m->version++;
        },
        true);
}

void text_view_lock(TextView* tv) {
    tv->locked = view_get_model(tv->view);
}

void text_view_set_ref(
    TextView* tv,
    uint8_t block,
    const char* text,
    size_t len,
    TextViewFont font) {
    TvModel* m = tv->locked;
    furi_check(m);
    if(block >= TV_BLOCKS) return;
    TvBlock* b = &m->block[block];
    b->ref = text;
    b->len = text ? len : 0;
    b->used = text && len;
    b->font = (uint8_t)font;
    m->version++;
}

void text_view_unlock(TextView* tv, bool redraw) {
    furi_check(tv->locked);
    tv->locked = NULL;
    view_commit_model(tv->view, redraw);
}

void text_view_set_footer(TextView* tv, const char* footer, const Icon* icon) {
    with_view_model(
        tv->view,
        TvModel * m,
        {
            snprintf(m->footer, sizeof(m->footer), "%s", footer ? footer : "");
            m->footer_icon = icon;
        },
        true);
}

void text_view_set_footer_hint(TextView* tv, const Icon* icon, const char* hint) {
    with_view_model(
        tv->view,
        TvModel * m,
        {
            snprintf(m->hint, sizeof(m->hint), "%s", hint ? hint : "");
            m->hint_icon = icon;
        },
        true);
}

void text_view_set_buttons(TextView* tv, const char* left, const char* center, const char* right) {
    with_view_model(
        tv->view,
        TvModel * m,
        {
            snprintf(m->btn[0], sizeof(m->btn[0]), "%s", left ? left : "");
            snprintf(m->btn[1], sizeof(m->btn[1]), "%s", center ? center : "");
            snprintf(m->btn[2], sizeof(m->btn[2]), "%s", right ? right : "");
        },
        true);
}

void text_view_set_follow_end(TextView* tv, bool follow) {
    with_view_model(tv->view, TvModel * m, { m->follow_end = follow; }, false);
}

void text_view_scroll_to_end(TextView* tv) {
    with_view_model(
        tv->view,
        TvModel * m,
        {
            m->scroll = 0xFFFF;
            m->at_end = true;
        },
        true);
}

void text_view_set_callback(TextView* tv, TextViewCallback cb, void* context) {
    tv->cb = cb;
    tv->cb_ctx = context;
}
