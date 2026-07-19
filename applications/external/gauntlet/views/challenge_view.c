#include "challenge_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

#define BODY_CAP    (CTF_PROMPT_LEN + CTF_DATA_LEN + CTF_HINT_LEN + 40)
#define LINE_CAP    40
#define MAX_LINES   56
#define BODY_TOP    26
#define BODY_BOT    54
#define BODY_LINE_H 9
#define VISIBLE     ((BODY_BOT - BODY_TOP) / BODY_LINE_H + 1) /* 4 lines */

struct ChallengeView {
    View* view;
    ChallengeViewCallback cb;
    void* ctx;
};

typedef struct {
    char title[CTF_TITLE_LEN];
    char body[BODY_CAP];
    CtfCategory category;
    CtfDifficulty difficulty;
    uint16_t points;
    bool solved;
    bool has_hint;
    bool hint_shown;
    int scroll;
    int total_lines;
} ChallengeModel;

/* Truncate a copy of src with an ellipsis so it fits max_w px in the font. */
static void fit_text(Canvas* canvas, const char* src, int max_w, char* out, size_t out_sz) {
    strncpy(out, src, out_sz - 1);
    out[out_sz - 1] = '\0';
    if(canvas_string_width(canvas, out) <= max_w) return;
    size_t len = strlen(out);
    while(len > 1) {
        len--;
        char probe[48];
        size_t n = (len < sizeof(probe) - 3) ? len : sizeof(probe) - 3;
        memcpy(probe, src, n);
        probe[n] = '.';
        probe[n + 1] = '.';
        probe[n + 2] = '\0';
        if(canvas_string_width(canvas, probe) <= max_w) {
            strncpy(out, probe, out_sz - 1);
            out[out_sz - 1] = '\0';
            return;
        }
    }
}

/* Greedy word-wrap that also honours embedded '\n'. Returns the line count. */
static int wrap_body(Canvas* canvas, const char* text, int max_w, char lines[][LINE_CAP]) {
    int nlines = 0;
    char cur[LINE_CAP];
    cur[0] = '\0';
    const char* p = text;
    char word[LINE_CAP];

    while(*p && nlines < MAX_LINES) {
        if(*p == '\n') {
            strncpy(lines[nlines], cur, LINE_CAP - 1);
            lines[nlines][LINE_CAP - 1] = '\0';
            nlines++;
            cur[0] = '\0';
            p++;
            continue;
        }
        if(*p == ' ') {
            p++;
            continue;
        }
        size_t wl = 0; /* read a word */
        while(*p && *p != ' ' && *p != '\n' && wl < LINE_CAP - 1) word[wl++] = *p++;
        word[wl] = '\0';

        char cand[LINE_CAP * 2];
        if(cur[0] == '\0')
            snprintf(cand, sizeof(cand), "%s", word);
        else
            snprintf(cand, sizeof(cand), "%s %s", cur, word);

        if(canvas_string_width(canvas, cand) <= max_w) {
            strncpy(cur, cand, LINE_CAP - 1);
            cur[LINE_CAP - 1] = '\0';
        } else {
            if(cur[0] != '\0' && nlines < MAX_LINES) {
                strncpy(lines[nlines], cur, LINE_CAP - 1);
                lines[nlines][LINE_CAP - 1] = '\0';
                nlines++;
            }
            /* the word alone may still exceed max_w; it just gets clipped by fit */
            strncpy(cur, word, LINE_CAP - 1);
            cur[LINE_CAP - 1] = '\0';
        }
    }
    if(cur[0] != '\0' && nlines < MAX_LINES) {
        strncpy(lines[nlines], cur, LINE_CAP - 1);
        lines[nlines][LINE_CAP - 1] = '\0';
        nlines++;
    }
    return nlines;
}

static void draw_check(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x, y + 2, x + 2, y + 4);
    canvas_draw_line(canvas, x + 2, y + 4, x + 6, y);
}

static void challenge_view_draw(Canvas* canvas, void* model) {
    ChallengeModel* m = model;
    canvas_clear(canvas);

    /* ---- header strip (inverted) ---- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 9, ctf_category_tag(m->category));

    /* difficulty pips, centred */
    int pips = (int)m->difficulty + 1;
    int pw = 4, gap = 2, total = 4 * pw + 3 * gap;
    int px = 64 - total / 2;
    for(int i = 0; i < 4; i++) {
        int x = px + i * (pw + gap);
        if(i < pips)
            canvas_draw_box(canvas, x, 4, pw, pw);
        else
            canvas_draw_frame(canvas, x, 4, pw, pw);
    }

    /* points + solved tick, right */
    char pts[10];
    snprintf(pts, sizeof(pts), "%up", m->points);
    canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, pts);
    if(m->solved) {
        int w = canvas_string_width(canvas, pts);
        draw_check(canvas, 125 - w - 12, 2);
    }

    /* ---- title ---- */
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    char title[CTF_TITLE_LEN + 4];
    fit_text(canvas, m->title, 126, title, sizeof(title));
    canvas_draw_str(canvas, 2, 22, title);

    /* ---- body (wrapped + scrolled) ---- */
    canvas_set_font(canvas, FontSecondary);
    static char lines[MAX_LINES][LINE_CAP];
    int n = wrap_body(canvas, m->body, 118, lines);
    m->total_lines = n;
    if(m->scroll > n - (int)VISIBLE) m->scroll = n - (int)VISIBLE;
    if(m->scroll < 0) m->scroll = 0;

    int y = BODY_TOP;
    for(int i = 0; i < (int)VISIBLE; i++) {
        int idx = m->scroll + i;
        if(idx >= n) break;
        canvas_draw_str(canvas, 2, y, lines[idx]);
        y += BODY_LINE_H;
    }
    if(n > (int)VISIBLE) {
        elements_scrollbar_pos(canvas, 125, BODY_TOP - 6, 32, m->scroll, n - (int)VISIBLE + 1);
    }

    /* ---- footer (inverted) ---- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 56, 128, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "OK Flag");
    if(m->has_hint) {
        const char* h = m->hint_shown ? "<Hide" : "<Hint";
        canvas_draw_str_aligned(canvas, 68, 63, AlignCenter, AlignBottom, h);
    }
    canvas_draw_str_aligned(canvas, 125, 63, AlignRight, AlignBottom, "Tools>");
    canvas_set_color(canvas, ColorBlack);
}

static bool challenge_view_input(InputEvent* event, void* context) {
    ChallengeView* v = context;
    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                v->view, ChallengeModel * m, { if(m->scroll > 0) m->scroll--; }, true);
            return true;
        }
        if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                ChallengeModel * m,
                { if(m->scroll < m->total_lines - (int)VISIBLE) m->scroll++; },
                true);
            return true;
        }
    }
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        if(v->cb) v->cb(v->ctx, ChallengeEventFlag);
        return true;
    }
    if(event->key == InputKeyRight) {
        if(v->cb) v->cb(v->ctx, ChallengeEventTools);
        return true;
    }
    if(event->key == InputKeyLeft) {
        if(v->cb) v->cb(v->ctx, ChallengeEventHint);
        return true;
    }
    return false; /* Back falls through to navigation */
}

ChallengeView* challenge_view_alloc(void) {
    ChallengeView* v = malloc(sizeof(ChallengeView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ChallengeModel));
    view_set_draw_callback(v->view, challenge_view_draw);
    view_set_input_callback(v->view, challenge_view_input);
    return v;
}

void challenge_view_free(ChallengeView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* challenge_view_get_view(ChallengeView* v) {
    furi_assert(v);
    return v->view;
}

void challenge_view_set_callback(ChallengeView* v, ChallengeViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

void challenge_view_set(
    ChallengeView* v,
    const char* title,
    const char* body,
    CtfCategory category,
    CtfDifficulty difficulty,
    uint16_t points,
    bool solved,
    bool has_hint,
    bool hint_shown) {
    furi_assert(v);
    with_view_model(
        v->view,
        ChallengeModel * m,
        {
            strncpy(m->title, title, sizeof(m->title) - 1);
            m->title[sizeof(m->title) - 1] = '\0';
            strncpy(m->body, body, sizeof(m->body) - 1);
            m->body[sizeof(m->body) - 1] = '\0';
            m->category = category;
            m->difficulty = difficulty;
            m->points = points;
            m->solved = solved;
            m->has_hint = has_hint;
            m->hint_shown = hint_shown;
            m->scroll = 0;
            m->total_lines = 0;
        },
        true);
}
