#include "toolkit_view.h"
#include "../helpers/ctf_codec.h"
#include "../helpers/ctf_pack.h" /* CTF_DATA_LEN */
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

#define TK_SRC_CAP  CTF_DATA_LEN
#define TK_OUT_CAP  CTF_DATA_LEN
#define TK_LINE_CAP 40
#define TK_MAX_LINE 40
#define TK_TOP      26
#define TK_LINE_H   9
#define TK_VISIBLE  3

struct ToolkitView {
    View* view;
    ToolkitViewCallback cb;
    void* ctx;
};

typedef struct {
    char src[TK_SRC_CAP];
    char out[TK_OUT_CAP];
    CodecId codec;
    int rot;
    int scroll;
    int total_lines;
} ToolkitModel;

static void recompute(ToolkitModel* m) {
    codec_apply(m->codec, m->src, m->rot, m->out, sizeof(m->out));
    m->scroll = 0;
}

static int wrap_out(Canvas* canvas, const char* text, int max_w, char lines[][TK_LINE_CAP]) {
    int nlines = 0;
    char cur[TK_LINE_CAP];
    cur[0] = '\0';
    const char* p = text;
    char word[TK_LINE_CAP];
    while(*p && nlines < TK_MAX_LINE) {
        if(*p == '\n') {
            strncpy(lines[nlines], cur, TK_LINE_CAP - 1);
            lines[nlines][TK_LINE_CAP - 1] = '\0';
            nlines++;
            cur[0] = '\0';
            p++;
            continue;
        }
        if(*p == ' ') {
            p++;
            continue;
        }
        size_t wl = 0;
        while(*p && *p != ' ' && *p != '\n' && wl < TK_LINE_CAP - 1) word[wl++] = *p++;
        word[wl] = '\0';
        char cand[TK_LINE_CAP * 2];
        if(cur[0] == '\0')
            snprintf(cand, sizeof(cand), "%s", word);
        else
            snprintf(cand, sizeof(cand), "%s %s", cur, word);
        if(canvas_string_width(canvas, cand) <= max_w) {
            strncpy(cur, cand, TK_LINE_CAP - 1);
            cur[TK_LINE_CAP - 1] = '\0';
        } else {
            if(cur[0] != '\0' && nlines < TK_MAX_LINE) {
                strncpy(lines[nlines], cur, TK_LINE_CAP - 1);
                lines[nlines][TK_LINE_CAP - 1] = '\0';
                nlines++;
            }
            strncpy(cur, word, TK_LINE_CAP - 1);
            cur[TK_LINE_CAP - 1] = '\0';
        }
    }
    if(cur[0] != '\0' && nlines < TK_MAX_LINE) {
        strncpy(lines[nlines], cur, TK_LINE_CAP - 1);
        lines[nlines][TK_LINE_CAP - 1] = '\0';
        nlines++;
    }
    return nlines;
}

static void draw_src_preview(Canvas* canvas, const char* src) {
    char buf[40];
    /* "in: " + as much of src as fits, built by hand to dodge -Wformat-truncation */
    size_t n = 0;
    const char* pfx = "in: ";
    while(*pfx && n < sizeof(buf) - 1) buf[n++] = *pfx++;
    while(*src && n < sizeof(buf) - 1) buf[n++] = *src++;
    buf[n] = '\0';
    /* fit to width with a trailing ellipsis */
    while(n > 4 && canvas_string_width(canvas, buf) > 122) {
        n--;
        buf[n] = '\0';
        if(n >= 2) {
            buf[n - 1] = '.';
            buf[n - 2] = '.';
        }
    }
    canvas_draw_str(canvas, 2, 21, buf);
}

static void toolkit_view_draw(Canvas* canvas, void* model) {
    ToolkitModel* m = model;
    canvas_clear(canvas);

    /* header strip */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 9, "<");
    canvas_draw_str_aligned(canvas, 122, 9, AlignRight, AlignBottom, ">");
    canvas_draw_str_aligned(
        canvas, 64, 9, AlignCenter, AlignBottom, codec_name(m->codec, m->rot));

    /* source preview */
    canvas_set_color(canvas, ColorBlack);
    draw_src_preview(canvas, m->src);
    canvas_draw_line(canvas, 0, 23, 128, 23);

    /* output, wrapped + scrolled */
    static char lines[TK_MAX_LINE][TK_LINE_CAP];
    int n = wrap_out(canvas, m->out, 122, lines);
    m->total_lines = n;
    if(m->scroll > n - TK_VISIBLE) m->scroll = n - TK_VISIBLE;
    if(m->scroll < 0) m->scroll = 0;

    if(n == 0) {
        canvas_draw_str(canvas, 2, TK_TOP + 6, "(no output)");
    } else {
        int y = TK_TOP + 6;
        for(int i = 0; i < TK_VISIBLE; i++) {
            int idx = m->scroll + i;
            if(idx >= n) break;
            canvas_draw_str(canvas, 2, y, lines[idx]);
            y += TK_LINE_H;
        }
        if(n > TK_VISIBLE)
            elements_scrollbar_pos(canvas, 125, TK_TOP, 28, m->scroll, n - TK_VISIBLE + 1);
    }

    /* footer */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 56, 128, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    if(codec_uses_param(m->codec))
        canvas_draw_str(canvas, 2, 63, "UpDn Shift");
    else
        canvas_draw_str(canvas, 2, 63, "UpDn Scroll");
    canvas_draw_str_aligned(canvas, 125, 63, AlignRight, AlignBottom, "OK Use");
    canvas_set_color(canvas, ColorBlack);
}

static bool toolkit_view_input(InputEvent* event, void* context) {
    ToolkitView* v = context;
    bool handled = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyRight) {
            with_view_model(
                v->view,
                ToolkitModel * m,
                {
                    m->codec = (CodecId)((m->codec + 1) % CodecCount);
                    recompute(m);
                },
                true);
            handled = true;
        } else if(event->key == InputKeyLeft) {
            with_view_model(
                v->view,
                ToolkitModel * m,
                {
                    m->codec = (CodecId)((m->codec + CodecCount - 1) % CodecCount);
                    recompute(m);
                },
                true);
            handled = true;
        } else if(event->key == InputKeyUp) {
            with_view_model(
                v->view,
                ToolkitModel * m,
                {
                    if(codec_uses_param(m->codec)) {
                        m->rot = (m->rot + 1) % 26;
                        codec_apply(m->codec, m->src, m->rot, m->out, sizeof(m->out));
                    } else if(m->scroll > 0) {
                        m->scroll--;
                    }
                },
                true);
            handled = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                ToolkitModel * m,
                {
                    if(codec_uses_param(m->codec)) {
                        m->rot = (m->rot + 25) % 26;
                        codec_apply(m->codec, m->src, m->rot, m->out, sizeof(m->out));
                    } else if(m->scroll < m->total_lines - TK_VISIBLE) {
                        m->scroll++;
                    }
                },
                true);
            handled = true;
        }
    }
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->cb) v->cb(v->ctx, ToolkitEventUse);
        handled = true;
    }
    return handled; /* Back falls through to navigation */
}

ToolkitView* toolkit_view_alloc(void) {
    ToolkitView* v = malloc(sizeof(ToolkitView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ToolkitModel));
    view_set_draw_callback(v->view, toolkit_view_draw);
    view_set_input_callback(v->view, toolkit_view_input);
    return v;
}

void toolkit_view_free(ToolkitView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* toolkit_view_get_view(ToolkitView* v) {
    furi_assert(v);
    return v->view;
}

void toolkit_view_set_callback(ToolkitView* v, ToolkitViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

void toolkit_view_set_source(ToolkitView* v, const char* src) {
    furi_assert(v);
    with_view_model(
        v->view,
        ToolkitModel * m,
        {
            strncpy(m->src, src ? src : "", sizeof(m->src) - 1);
            m->src[sizeof(m->src) - 1] = '\0';
            m->codec = CodecRot;
            m->rot = 13;
            recompute(m);
        },
        true);
}

void toolkit_view_get_output(ToolkitView* v, char* buf, size_t sz) {
    furi_assert(v);
    with_view_model(
        v->view,
        ToolkitModel * m,
        {
            strncpy(buf, m->out, sz - 1);
            buf[sz - 1] = '\0';
        },
        false);
}
