/* Render test: the whole app on the host (fake_gui/), driven like the on-device walks
 * (tools/flipper/walks.py) plus a few extra paths, with every frame checked:
 *
 *   - text stays on the screen and inside the 2 px side margins (stock buttons aside);
 *   - no two texts overlap; no text touches an icon, a frame, a line or the scrollbar;
 *   - black text never sits on a black box, white text never outside one;
 *   - no text is partly erased by something drawn after it;
 *   - stock multi-line text never needs the firmware's own wrap (it breaks mid-word);
 *   - TextView never breaks a word that fits on a line;
 *   - fonts per role: titles FontPrimary (plain words) or FontKeyboard (technical text),
 *     body FontSecondary or FontKeyboard, buttons FontSecondary;
 *   - technical text (addresses, commands, paths, '@', '/', ...: text_is_technical*) is
 *     never in a proportional font in the app's own drawing;
 *     stock modules (Submenu, VariableItemList) choose their own font and are listed;
 *   - every scene is entered at least once.
 *
 * Frames named like the device screenshots are written to render_frames.txt (text ops
 * with their ink boxes) for tools/flipper/render_calibrate.py.
 *
 * SPDX-License-Identifier: GPL-3.0-only */
#include "fake_gui/fake_test.h"
#include "../ssh_runner_i.h"
#include "scene_util.h"
#include "kbd_layout.h"
#include "fontw.h"

#include <ctype.h>

/* ---------------- scene names ---------------- */

#define ADD_SCENE(prefix, name, id) #id,
static const char* const scene_names[] = {
#include "../scenes/ssh_runner_scene_config.h"
};
#undef ADD_SCENE

/* ---------------- text the app gave to the TextView (ld --wrap) ---------------- */

#define SRC_MAX 9000
static char tv_src[TEXT_VIEW_BLOCKS][SRC_MAX];

void __real_text_view_set_text(TextView* tv, uint8_t block, const char* text, TextViewFont font);
void __wrap_text_view_set_text(TextView* tv, uint8_t block, const char* text, TextViewFont font) {
    if(block < TEXT_VIEW_BLOCKS) snprintf(tv_src[block], SRC_MAX, "%s", text ? text : "");
    __real_text_view_set_text(tv, block, text, font);
}
void __real_text_view_set_ref(
    TextView* tv,
    uint8_t block,
    const char* text,
    size_t len,
    TextViewFont font);
void __wrap_text_view_set_ref(
    TextView* tv,
    uint8_t block,
    const char* text,
    size_t len,
    TextViewFont font) {
    if(block < TEXT_VIEW_BLOCKS)
        snprintf(tv_src[block], SRC_MAX, "%.*s", text ? (int)len : 0, text ? text : "");
    __real_text_view_set_ref(tv, block, text, len, font);
}
void __real_text_view_reset(TextView* tv);
void __wrap_text_view_reset(TextView* tv) {
    for(int b = 0; b < TEXT_VIEW_BLOCKS; b++)
        tv_src[b][0] = 0;
    __real_text_view_reset(tv);
}

/* ---------------- checks ---------------- */

static int failures, frames_checked;
static char where[96];
static char cut_seen[64][64];
static int n_cut_seen;

#define FAIL(...)                                                                                  \
    do {                                                                                           \
        if(failures < 60) {                                                                        \
            fprintf(                                                                               \
                stderr,                                                                            \
                "FAIL [%s, %s] ",                                                                  \
                where,                                                                             \
                scene_names[fake_current_scene() < SshRunnerSceneNum ? fake_current_scene() : 0]); \
            fprintf(stderr, __VA_ARGS__);                                                          \
            fputc('\n', stderr);                                                                   \
        }                                                                                          \
        failures++;                                                                                \
    } while(0)

static bool is_text(const Op* o) {
    return o->kind == OpText || o->kind == OpGlyph;
}
static bool hit(const Op* a, int x, int y, int w, int h) {
    return a->x < x + w && x < a->x + a->w && a->y < y + h && y < a->y + a->h;
}
static bool within(const Op* a, const Op* b) {
    return a->x >= b->x && a->y >= b->y && a->x + a->w <= b->x + b->w &&
           a->y + a->h <= b->y + b->h;
}
static bool frame_edge_hit(const Op* t, const Op* f) {
    return hit(t, f->x, f->y, f->w, 1) || hit(t, f->x, f->y + f->h - 1, f->w, 1) ||
           hit(t, f->x, f->y, 1, f->h) || hit(t, f->x + f->w - 1, f->y, 1, f->h);
}

/* TextView: a line may end inside a word only when that word is wider than a line */
/* TextView scrollbar: with the text's last line in view the thumb ends at the bottom of the
 * track; with its first line in view it starts at the top (38 looked finished at 1/4). */
static bool ends_with(const char* s, const char* tail) {
    size_t n = strlen(s), k = strlen(tail);
    while(n && (s[n - 1] == '\n' || s[n - 1] == ' '))
        n--;
    return k && k <= n && !strncmp(s + n - k, tail, k);
}
static void check_textview_thumb(const Frame* fr) {
    const Op *track = NULL, *thumb = NULL, *first = NULL, *last = NULL;
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->kind == OpBox && o->x == 125 && o->w == 3) {
            if(o->color == ColorWhite)
                track = o;
            else
                thumb = o;
        }
        if(o->kind == OpText && o->role == RoleOwn && o->bx == 2 && o->by > 12 && o->by < 52) {
            if(!first) first = o;
            last = o;
        }
    }
    if(!track || !thumb || !first) return;
    const char* end_src = tv_src[0];
    for(int b = 1; b < TEXT_VIEW_BLOCKS; b++)
        if(tv_src[b][0]) end_src = tv_src[b];
    const char* start_src = tv_src[0];
    for(int b = 0; b < TEXT_VIEW_BLOCKS && !start_src[0]; b++)
        start_src = tv_src[b];
    while(*start_src == '\n' || *start_src == ' ')
        start_src++;
    if(ends_with(end_src, last->text) && thumb->y + thumb->h != track->y + track->h)
        FAIL(
            "last line \"%s\" in view but the thumb ends at %d, not %d",
            last->text,
            thumb->y + thumb->h,
            track->y + track->h);
    if(!strncmp(start_src, first->text, strlen(first->text)) && thumb->y != track->y)
        FAIL(
            "first line \"%s\" in view but the thumb starts at %d, not %d",
            first->text,
            thumb->y,
            track->y);
}

static void check_textview_words(const Frame* fr) {
    const int width = 128 - 2 * 2 - 3;
    int prev_end = -1, prev_block = -1;
    const char* src = NULL;
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->kind != OpText || o->role != RoleOwn || o->bx != 2 || o->by < 14 || o->by > 62)
            continue;
        if(o->font == FontPrimary) continue;
        /* find this line in block 0 or 1, after the previous line */
        int block = -1, pos = -1;
        for(int b = 0; b < TEXT_VIEW_BLOCKS && block < 0; b++) {
            const char* from = tv_src[b] + (b == prev_block && prev_end >= 0 ? prev_end : 0);
            const char* p = strstr(from, o->text);
            if(p) {
                block = b;
                pos = (int)(p - tv_src[b]);
            }
        }
        if(block < 0) {
            prev_end = -1;
            continue;
        }
        src = tv_src[block];
        if(block == prev_block && prev_end == pos && pos > 0 &&
           isalnum((unsigned char)src[pos - 1]) && isalnum((unsigned char)src[pos])) {
            int a = pos, b = pos;
            while(a > 0 && !isspace((unsigned char)src[a - 1]))
                a--;
            while(src[b] && !isspace((unsigned char)src[b]))
                b++;
            char word[128];
            snprintf(word, sizeof(word), "%.*s", b - a, src + a);
            if(fake_text_width(o->font, word) <= width)
                FAIL("word \"%s\" broken across lines", word);
        }
        prev_block = block;
        prev_end = pos + (int)strlen(o->text);
    }
}

/* Names without symbols that are still technical (an SSID, an alias): mono on purpose. */
static bool mono_title_ok(const char* t) {
    return !strchr(t, ' ');
}

static void check_fonts(const Frame* fr) {
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->hidden || !is_text(o) || o->role == RoleButton) continue;
        switch(fr->view_id) {
        case ViewText:
        case ViewProgress:
            if(o->by <= 13 && o->font != FontPrimary && o->font != FontKeyboard)
                FAIL("title \"%s\" in font %d", o->text, o->font);
            if(o->by <= 13 && o->font == FontKeyboard && !text_is_technical(o->text) &&
               fr->view_id == ViewText && !mono_title_ok(o->text))
                FAIL("plain-word title \"%s\" in FontKeyboard", o->text);
            if(o->by > 12 && o->font != FontSecondary && o->font != FontKeyboard)
                FAIL("body \"%s\" in font %d", o->text, o->font);
            break;
        case ViewKbd:
            /* title in words (FontSecondary) or a format (FontKeyboard, technical text) */
            if(o->by <= 7 && o->font != FontSecondary &&
               !(o->font == FontKeyboard && text_is_technical(o->text)))
                FAIL("keyboard title \"%s\" font %d", o->text, o->font);
            /* typed text and examples in FontKeyboard; the words "e.g." in FontSecondary */
            if(o->by > 7 && o->by <= 20 && o->font != FontKeyboard && strcmp(o->text, "e.g."))
                FAIL("field \"%s\" font %d", o->text, o->font);
            break;
        case ViewHome:
            /* menu and words in FontSecondary; the address in the header in FontKeyboard */
            if(o->font != FontSecondary && !(o->font == FontKeyboard && o->by <= 11))
                FAIL("home \"%s\" font %d", o->text, o->font);
            break;
        default:
            break;
        }
    }
}

static char exempt_seen[16][100];
static int n_exempt;
static void note_exemption(const char* text) {
    for(int j = 0; j < n_exempt; j++)
        if(!strcmp(exempt_seen[j], text)) return;
    if(n_exempt < 16) snprintf(exempt_seen[n_exempt++], 100, "%s", text);
}

/* technical text in a stock module's proportional font (the module picks it) */
static char stock_tech_seen[24][64];
static int n_stock_tech;
static void note_stock_tech(const char* text) {
    for(int j = 0; j < n_stock_tech; j++)
        if(!strcmp(stock_tech_seen[j], text)) return;
    if(n_stock_tech < 24) snprintf(stock_tech_seen[n_stock_tech++], 64, "%s", text);
}

static void check_technical(const Frame* fr) {
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->hidden || o->kind != OpText || o->role == RoleButton) continue;
        bool bad = o->font == FontPrimary   ? text_is_technical(o->text) :
                   o->font == FontSecondary ? text_is_technical_body(o->text) :
                                              false;
        if(!bad) continue;
        if(o->role == RoleStock)
            note_stock_tech(o->text);
        else
            FAIL(
                "technical text \"%s\" in proportional font %d (FontKeyboard only)",
                o->text,
                o->font);
    }
}

/* Keyboard field (frame rows 8..19): nothing inks the row under its top edge (the '|'
 * cursor glyph did). The bottom cannot keep a row: 10 inner rows hold 7 rows of caps
 * and 2 of descenders, so descenders may reach row 18 (as in the stock TextInput). */
static void check_kbd_field(const Frame* fr) {
    if(fr->view_id != ViewKbd) return;
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->hidden || (o->kind != OpText && o->kind != OpLine && o->kind != OpBox)) continue;
        if(o->x < 1 || o->x + o->w > 127 || o->y < 9 || o->y > 18) continue; /* inside the field */
        if(o->y < 10)
            FAIL(
                "\"%s\" (%s) touches the field's top edge",
                o->text,
                o->kind == OpLine ? "line" : "ink");
    }
    /* A prefilled (highlighted) value: one plain rectangle at least 1 px inside the frame, and
     * everything drawn on it white, so no black ink sticks out below it (the '_' "bump").
     * Checked in both states: without a highlight nothing here applies. */
    int box = -1;
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        /* the highlight starts at x 3 (the block cursor of position editing is elsewhere) */
        if(!o->hidden && o->kind == OpBox && o->color == ColorBlack && o->x == 3 && o->y >= 9 &&
           o->y + o->h <= 19)
            box = i;
    }
    if(box < 0) return;
    const Op* b = &fr->op[box];
    if(b->y < 10 || b->y + b->h > 18)
        FAIL(
            "prefill highlight at rows %d..%d touches the field frame (8..19)",
            b->y,
            b->y + b->h - 1);
    for(int i = box + 1; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->hidden || o->y > 19 || o->y + o->h < 9) continue;
        if((o->kind == OpText || o->kind == OpLine) && o->color != ColorWhite)
            FAIL(
                "\"%s\" drawn on the prefill highlight in black/XOR: ink would stick out below it",
                o->text);
        if(o->kind == OpLine && (o->y < b->y || o->y >= b->y + b->h))
            FAIL("a line on the highlight leaves its rectangle");
    }
}

static void check_frame(const Frame* fr) {
    frames_checked++;
    if(fr->bad_button_font) FAIL("stock button drawn with a font other than FontSecondary");
    if(fr->space_dots) FAIL("a stock \"...\" cut leaves a space before the dots");
    for(int k = 0; k < fr->n_fit_cut; k++) {
        if(fr->fit_cut[k][0] == '\x02') {
            FAIL("stock multi-line text had to wrap a line by itself (mid-word, with '-')");
            continue;
        }
        bool seen = false;
        for(int j = 0; j < n_cut_seen; j++)
            seen |= !strcmp(cut_seen[j], fr->fit_cut[k]);
        if(!seen && n_cut_seen < 64) snprintf(cut_seen[n_cut_seen++], 64, "%s", fr->fit_cut[k]);
    }
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->hidden) continue;
        /* Accepted community keyboard geometry (title at y 7, field frame at y 8):
         * a title glyph taller than a capital ('@', brackets) loses its top row and
         * descenders touch the field frame. Counted and listed, not failed. */
        bool kbd_title = fr->view_id == ViewKbd && is_text(o) && o->by == 7;
        if(kbd_title && (o->y < 0 || o->y + o->h > 8)) {
            note_exemption(o->text);
            continue;
        }
        if(o->x < 0 || o->y < 0 || o->x + o->w > 128 || o->y + o->h > 64)
            FAIL(
                "off screen: %s \"%s\" at %d,%d %dx%d",
                is_text(o) ? "text" : "shape",
                o->text,
                o->x,
                o->y,
                o->w,
                o->h);
        if(!is_text(o)) continue;
        if(o->clipped) FAIL("text \"%s\" partly erased by a later box", o->text);
        /* every glyph's ink (cap height, brackets, descenders) keeps 2 px from all four edges;
         * the keyboard (community layout) and stock button labels aside. Shapes are not
         * margin-checked: scrollbars, button bars and the home header rule use the stock
         * edge geometry (elements_scrollbar_pos at x 125..127, status-bar rule). */
        if(fr->view_id != ViewKbd && o->role != RoleButton &&
           (o->x < 2 || o->y < 2 || o->x + o->w > 126 || o->y + o->h > 62))
            FAIL(
                "text \"%s\" ink %d,%d..%d,%d inside the 2 px margin",
                o->text,
                o->x,
                o->y,
                o->x + o->w - 1,
                o->y + o->h - 1);
        if(o->y <= 0 || o->y + o->h >= 64) {
            if(fr->view_id == ViewKbd)
                note_exemption(
                    o->text); /* community keyboard: title caps on row 0, bottom keys on 63 */
            else
                FAIL("text \"%s\" inks row 0 or row 63", o->text);
        }
        for(int j = 0; j < fr->n; j++) {
            const Op* s = &fr->op[j];
            if(j == i || s->hidden) continue;
            if(is_text(s)) {
                if(j > i && hit(o, s->x, s->y, s->w, s->h))
                    FAIL("texts overlap: \"%s\" and \"%s\"", o->text, s->text);
                continue;
            }
            switch(s->kind) {
            case OpBox:
                /* (XOR text is visible on both) */
                if(s->color == ColorBlack && o->color == ColorBlack &&
                   hit(o, s->x, s->y, s->w, s->h))
                    FAIL(
                        "black text \"%s\" on a black %s",
                        o->text,
                        s->role == RoleScrollbar ? "scrollbar" : "box");
                break;
            case OpFrame:
                if(frame_edge_hit(o, s)) FAIL("text \"%s\" touches a frame", o->text);
                break;
            case OpLine:
                /* the keyboard prefill draws '_' as white lines on its highlight */
                if(fr->view_id == ViewKbd && s->color == ColorWhite && o->color == ColorWhite)
                    break;
                if(hit(o, s->x, s->y, s->w, s->h)) FAIL("text \"%s\" touches a line", o->text);
                break;
            case OpIcon:
                if(hit(o, s->x, s->y, s->w, s->h))
                    FAIL("text \"%s\" touches icon %s", o->text, s->text);
                break;
            default:
                break;
            }
        }
        if(o->color == ColorWhite) {
            bool on_black = false;
            for(int j = 0; j < fr->n && !on_black; j++) {
                const Op* s = &fr->op[j];
                on_black = !s->hidden && s->kind == OpBox && s->color == ColorBlack &&
                           within(o, s);
            }
            /* keyboard prefill (baseline 17): caps on the box; its descender rows below it are
             * white on the background, i.e. not drawn, as in the stock TextInput */
            bool kbd_prefill = fr->view_id == ViewKbd && o->by == 17;
            if(!on_black && !kbd_prefill) FAIL("white text \"%s\" not on a black box", o->text);
        }
    }
    check_fonts(fr);
    check_technical(fr);
    check_kbd_field(fr);
    if(fr->view_id == ViewText) {
        check_textview_words(fr);
        check_textview_thumb(fr);
    }
}

static FILE* dump;
static void step(void) {
    check_frame(fake_render());
}
/* The current frame shows `text` as one whole string (a row label, not a scrolling value). */
static void expect_text(const char* text) {
    const Frame* fr = fake_render();
    for(int i = 0; i < fr->n; i++)
        if(!fr->op[i].hidden && fr->op[i].kind == OpText && !strcmp(fr->op[i].text, text)) return;
    FAIL("\"%s\" not drawn whole", text);
}

/* A key cap (canvas_draw_glyph) showing `c`. */
static void expect_key(char c) {
    const Frame* fr = fake_render();
    for(int i = 0; i < fr->n; i++)
        if(!fr->op[i].hidden && fr->op[i].kind == OpGlyph && fr->op[i].text[0] == c) return;
    FAIL("key '%c' not drawn", c);
}

/* Some drawn text holds `part` (or, with `want` false, none does). */
static void expect_part(const char* part, bool want) {
    const Frame* fr = fake_render();
    bool seen = false;
    for(int i = 0; i < fr->n && !seen; i++)
        seen = !fr->op[i].hidden && fr->op[i].kind == OpText && strstr(fr->op[i].text, part);
    if(seen != want) FAIL("\"%s\" %s", part, want ? "not drawn" : "still drawn");
}

/* A "label   value" row (util_label_flush): drawn whole, the value's ink ending under the stock
 * '>' (x 6 + LABEL_VALUE_RIGHT_PX), like the value column of the rows around it. */
static void expect_flush(const char* label, const char* value) {
    const Frame* fr = fake_render();
    size_t ll = strlen(label), vl = strlen(value);
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        size_t n = strlen(o->text);
        if(o->hidden || o->kind != OpText || n < ll + vl + 1 || strncmp(o->text, label, ll) ||
           strcmp(o->text + n - vl, value))
            continue;
        for(size_t k = ll; k < n - vl; k++)
            if(o->text[k] != ' ') FAIL("\"%s\": not label, spaces, value", o->text);
        int right = o->x + o->w, want = 6 + LABEL_VALUE_RIGHT_PX;
        if(right < want - 2 || right > want + 1)
            FAIL("\"%s\" ends at x %d, not %d", o->text, right, want);
        return;
    }
    FAIL("\"%s ... %s\" not drawn whole", label, value);
}

static void shot(const char* name) {
    const Frame* fr = fake_render();
    check_frame(fr);
    if(!dump) return;
    fprintf(dump, "frame %s view %u\n", name, (unsigned)fr->view_id);
    for(int i = 0; i < fr->n; i++) {
        const Op* o = &fr->op[i];
        if(o->hidden) continue;
        static const char* const kinds[] = {"text", "glyph", "box", "frame", "line", "icon"};
        fprintf(
            dump,
            "  %s %d %d %d %d %d %d %s\n",
            kinds[o->kind],
            o->x,
            o->y,
            o->w,
            o->h,
            o->color,
            o->font,
            o->text);
    }
}

/* ---------------- driving ---------------- */

static void press(InputKey k, int n) {
    for(int i = 0; i < n && fake_running(); i++) {
        fake_press(k);
        step();
    }
}
static void longp(InputKey k) {
    fake_long(k);
    step();
}
static void wait_ms(uint32_t ms) {
    while(ms && fake_running()) {
        uint32_t s = ms < 50 ? ms : 50;
        fake_advance(s);
        ms -= s;
        step();
    }
}
#define UP    InputKeyUp
#define DOWN  InputKeyDown
#define LEFT  InputKeyLeft
#define RIGHT InputKeyRight
#define OK    InputKeyOk
#define BACK  InputKeyBack

static uint8_t kr = 1, kc = 0; /* keyboard cursor: starts on 'q' */

/* shortest key path from (kr, kc) to the key that types `c` (short) or its shift (long) */
static void kbd_go(char c) {
    uint8_t dist[KBD_ROWS][KBD_MAXKEYS];
    uint8_t from[KBD_ROWS][KBD_MAXKEYS][3]; /* prev row, prev col, key */
    memset(dist, 0xFF, sizeof(dist));
    uint8_t qr[64], qc[64];
    int qh = 0, qt = 0;
    dist[kr][kc] = 0;
    qr[qt] = kr;
    qc[qt++] = kc;
    int best_r = -1, best_c = -1;
    bool best_long = false;
    while(qh < qt) {
        uint8_t r = qr[qh], col = qc[qh++];
        uint8_t n;
        const KbdKey* row = kbd_row(r, &n);
        char k = row[col].key;
        if(k == c || (kbd_shift(k) == c && k != c)) {
            best_r = r;
            best_c = col;
            best_long = k != c;
            break;
        }
        InputKey moves[4] = {LEFT, RIGHT, UP, DOWN};
        for(int m = 0; m < 4; m++) {
            int nr = r, nc = col;
            if(moves[m] == LEFT) nc = col ? col - 1 : n - 1;
            if(moves[m] == RIGHT) nc = col + 1 < n ? col + 1 : 0;
            if(moves[m] == UP) {
                if(!r) continue;
                nc = kbd_nearest(r, col, r - 1);
                nr = r - 1;
            }
            if(moves[m] == DOWN) {
                if(r + 1 >= KBD_ROWS) continue;
                nc = kbd_nearest(r, col, r + 1);
                nr = r + 1;
            }
            if(dist[nr][nc] != 0xFF) continue;
            dist[nr][nc] = dist[r][col] + 1;
            from[nr][nc][0] = r;
            from[nr][nc][1] = col;
            from[nr][nc][2] = (uint8_t)moves[m];
            qr[qt] = (uint8_t)nr;
            qc[qt++] = (uint8_t)nc;
        }
    }
    if(best_r < 0) {
        FAIL("keyboard has no key for '%c'", c);
        return;
    }
    InputKey path[64];
    int len = 0;
    for(int r = best_r, col = best_c; r != kr || col != kc;) {
        path[len++] = (InputKey)from[r][col][2];
        int pr = from[r][col][0], pc = from[r][col][1];
        r = pr;
        col = pc;
    }
    for(int i = len - 1; i >= 0; i--) {
        fake_press(path[i]);
    }
    kr = (uint8_t)best_r;
    kc = (uint8_t)best_c;
    if(best_long)
        fake_long(OK);
    else
        fake_press(OK);
    step();
}

/* type on a keyboard; fresh = it was just opened (cursor on 'q') */
static void type(const char* text, bool confirm, bool fresh) {
    if(fresh) {
        kr = 1;
        kc = 0;
    }
    for(const char* p = text; *p; p++)
        kbd_go(*p);
    if(confirm) kbd_go(KBD_ENTER);
}

static void exit_app(void) {
    for(int i = 0; i < 15 && fake_running(); i++) {
        if(i == 8) press(RIGHT, 1); /* "Discard Changes?" answers Back with Cancel */
        press(BACK, 1);
    }
    if(fake_running()) FAIL("app still running after 15 Back presses");
}

/* Peak heap per walk, from ASan's malloc hook: the Flipper crashes on the peak, not on
 * what a screenshot moment holds. Host pointers are 8 bytes, so this overstates a little. */
size_t __sanitizer_get_current_allocated_bytes(void);
int __sanitizer_install_malloc_and_free_hooks(
    void (*malloc_hook)(const volatile void*, size_t),
    void (*free_hook)(const volatile void*));
size_t __sanitizer_get_allocated_size(const volatile void* p);
static size_t heap_now, heap_peak; /* bytes, kept by the hooks (ASan's own counter lags) */
static void peak_hook(const volatile void* p, size_t n) {
    (void)p;
    size_t now = __atomic_add_fetch(&heap_now, n, __ATOMIC_RELAXED);
    size_t peak = __atomic_load_n(&heap_peak, __ATOMIC_RELAXED);
    while(now > peak && !__atomic_compare_exchange_n(
                            &heap_peak, &peak, now, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
}
static void free_hook(const volatile void* p) {
    if(p) __atomic_sub_fetch(&heap_now, __sanitizer_get_allocated_size(p), __ATOMIC_RELAXED);
}
static size_t walk_peak_max;
static const char* walk_peak_name = "";

static void run(const char* name, void (*walk)(void)) {
    snprintf(where, sizeof(where), "%s", name);
    fake_set_script(walk);
    size_t base = __atomic_load_n(&heap_now, __ATOMIC_RELAXED);
    __atomic_store_n(&heap_peak, base, __ATOMIC_RELAXED);
    fake_app_main();
    size_t peak = heap_peak - base;
    printf("render: peak heap in walk %s: %zu B\n", name, peak);
    if(peak > walk_peak_max) {
        walk_peak_max = peak;
        walk_peak_name = name;
    }
}

/* ---------------- walks (mirror tools/flipper/walks.py) ---------------- */

static void w_tour(void) {
    wait_ms(600);
    shot("01-home-no-board");
    press(DOWN, 5);
    press(OK, 1);
    shot("02-settings");
    press(OK, 1);
    shot("03-about");
    press(DOWN, 20);
    shot("03b-about-end"); /* author, website and data folder */
    expect_text("scx.tw/links");
    expect_text("apps_data/ssh_runner");
    press(BACK, 1);
    press(DOWN, 1);
    press(OK, 1);
    shot("04-developer");
    press(RIGHT, 1);
    wait_ms(800);
    shot("05-developer-mock-on");
    press(BACK, 2);
    wait_ms(500);
    shot("06-home-demo");
    exit_app();
}

static void w_states(void) {
    wait_ms(600);
    press(DOWN, 1);
    press(OK, 1);
    shot("60-hosts-empty");
    press(BACK, 1);
    press(DOWN, 1);
    press(OK, 1);
    shot("61-recent-empty");
    press(BACK, 1);
    press(UP, 2);
    press(OK, 1);
    type("nohost", true, true);
    shot("62-keyboard-error");
    press(LEFT, 1);
    exit_app();
}

static void w_quick_connect(void) {
    wait_ms(600);
    press(OK, 1);
    shot("07-quick-connect-empty");
    type("user@192.168.1.10", false, true);
    shot("08-quick-connect-typed");
    type("", true, false);
    shot("09-connecting");
    wait_ms(1000);
    shot("10-hostkey-new");
    press(RIGHT, 1);
    wait_ms(600);
    shot("11-password-empty");
    type("hunter2", false, true);
    shot("12-password-typed");
    type("", true, false);
    wait_ms(1200);
    shot("13-run-menu-quick");
    press(DOWN, 2);
    press(OK, 1);
    type("uname -a", true, true);
    wait_ms(1000);
    shot("14-output-uname");
    press(BACK, 1);
    wait_ms(300);
    shot("15-save-alias-offer");
    press(RIGHT, 1);
    shot("16-alias-suggested");
    type("nas", false, true);
    shot("16b-alias-typed");
    type("", true, false);
    wait_ms(300);
    shot("17-saved");
    press(OK, 1);
    shot("18-run-menu-saved-alias");
    exit_app();
}

/* the same as SAMPLE_SCRIPT in tools/flipper/walks.py */
static const char sample_script[] =
    "#!/bin/bash\n# sample script used by the on-device UI walk\nset -e\n"
    "echo \"backup: starting\"\nhostname\necho \"copied 128 files\"\n"
    "echo \"disk 61% used\"\necho \"backup: done\"\nexit 0\n";

static void w_saved_host(void) {
    wait_ms(600);
    press(DOWN, 1);
    press(OK, 1);
    shot("19-hosts");
    expect_text("nas"); /* the alias alone */
    press(OK, 1); /* the host page, on Connect */
    shot("20-host-page");
    expect_text("Connect");
    press(DOWN, 1); /* Alias */
    shot("21-host-edit");
    expect_flush("Host", "192.168.1.10"); /* in the label: no 37 px value column, no marquee */
    expect_flush("User", "user"); /* every typed value flush right, the row above too */
    press(DOWN, 2);
    shot("21b-host-selected");
    expect_flush("Host", "192.168.1.10"); /* selected: still whole */
    press(UP, 2);
    press(DOWN, 7);
    shot("22-host-edit-lower");
    press(DOWN, 1);
    press(OK, 1);
    shot("23-host-cmds-empty");
    press(OK, 1);
    type("sudo systemctl restart nginx", true, true);
    shot("24-host-cmds");
    press(UP, 1); /* from "+ Add Command" to the command */
    press(OK, 1); /* the whole command, Edit / Delete */
    shot("24b-command-detail-editor");
    press(BACK, 1);
    press(BACK, 1);
    press(DOWN, 1);
    press(OK, 1); /* Trusted keys (none yet for this alias) */
    wait_ms(400);
    shot("host-keys-empty");
    press(BACK, 1);
    press(DOWN, 1);
    press(OK, 1);
    wait_ms(300);
    shot("25-host-saved");
    press(OK, 1); /* the Hosts list */
    press(OK, 1); /* nas */
    press(OK, 1); /* Connect */
    wait_ms(1200);
    shot("26-hostkey-new-alias");
    press(RIGHT, 1);
    wait_ms(600);
    type("hunter2", true, true);
    wait_ms(1200);
    shot("27-run-menu-host");
    longp(OK); /* long-press: the whole command, Edit / Run / Delete */
    shot("27b-command-detail");
    press(BACK, 1);
    press(OK, 1);
    wait_ms(1000);
    shot("28-output-error");
    press(BACK, 1);
    press(DOWN, 4);
    press(OK, 1);
    shot("29-scripts");
    press(OK, 1);
    shot("30-script-confirm");
    press(RIGHT, 1);
    wait_ms(1200);
    shot("31-output-script");
    press(BACK, 1);
    press(UP, 1);
    press(OK, 1);
    type("seq 3000", true, true);
    wait_ms(600);
    shot("32-output-running");
    wait_ms(3500);
    shot("33-output-long-end");
    press(UP, 150);
    shot("34-output-long-scrolled");
    press(BACK, 1);
    press(DOWN, 2); /* Last output */
    press(OK, 1);
    wait_ms(300);
    shot("last-output");
    press(BACK, 1);
    press(DOWN, 1); /* Save last output */
    press(OK, 1);
    wait_ms(300);
    shot("35-output-saved");
    press(OK, 1);
    exit_app();
}

static void w_dialogs(void) {
    wait_ms(600);
    press(DOWN, 1);
    press(OK, 1);
    press(OK, 1); /* the host page */
    press(UP, 1); /* round to Delete, the last row */
    press(OK, 1);
    wait_ms(300);
    shot("63-confirm-delete-host");
    press(BACK, 3);
    press(DOWN, 4);
    press(OK, 1);
    press(DOWN, 1);
    press(OK, 1);
    press(DOWN, 3);
    press(OK, 1);
    wait_ms(1500);
    shot("64-report-linktest");
    press(OK, 1);
    exit_app();
}

static void w_fleet(void) {
    wait_ms(600);
    press(OK, 1);
    type("pi@192.168.4.1", true, true);
    wait_ms(1000);
    press(RIGHT, 1);
    wait_ms(600);
    type("raspberry", true, true);
    wait_ms(1200);
    press(BACK, 1);
    wait_ms(500);
    press(DOWN, 2);
    press(OK, 1);
    shot("36-recent");
    press(OK, 1);
    wait_ms(1200);
    shot("37-hostkey-changed");
    press(DOWN, 3);
    shot("38-hostkey-changed-scrolled");
    press(RIGHT, 1);
    shot("39-hostkey-options");
    press(OK, 1);
    wait_ms(500);
    shot("40-recent-after-cancel");
    longp(OK);
    shot("41-recent-menu");
    exit_app();
}

static void w_network(void) {
    wait_ms(600);
    press(DOWN, 3);
    press(OK, 1);
    wait_ms(300);
    shot("42-network");
    press(OK, 1);
    shot("43-lan");
    press(RIGHT, 1);
    wait_ms(300);
    shot("44-lan-static");
    expect_flush("Static IP", "192.168.1.50");
    expect_text("Static"); /* a < > choice keeps the stock value column */
    press(LEFT, 1);
    press(BACK, 1);
    press(DOWN, 2);
    press(OK, 1);
    wait_ms(300);
    shot("45-wifi-scanning");
    wait_ms(1800);
    shot("46-wifi-list");
    expect_text("Lab-WiFi"); /* from the first of the two scan events: the list holds both */
    press(DOWN, 5); /* the last one: a name outside ASCII, its characters as boxes */
    shot("46b-wifi-list-unicode");
    expect_text("WiFi");
    press(UP, 5);
    press(OK, 1);
    type("correct horse", false, true);
    expect_part("*e", true); /* the character just typed shows for a second */
    wait_ms(1100);
    expect_part("e", false);
    shot("47-wifi-password");
    type("", true, false);
    wait_ms(400);
    shot("48-wifi-joining");
    wait_ms(2200);
    shot("49-wifi-joined");
    press(OK, 1);
    shot("50-network-wifi-joined");
    press(UP, 1); /* WiFi */
    press(OK, 1);
    wait_ms(300);
    shot("wifi-iface");
    press(DOWN, 1); /* Ping, under Mode: prefilled with the gateway, through WiFi alone */
    press(OK, 1);
    expect_part("192.168.", true);
    press(OK, 1); /* the cursor starts on Save */
    wait_ms(1300);
    shot("51b-ping-gateway");
    expect_part("WiFi: 4 of 4", true);
    expect_part("LAN:", false);
    press(BACK, 1); /* back on the WiFi page */
    expect_text("Ping");
    press(BACK, 1);
    press(DOWN, 2); /* Ping host */
    press(OK, 1);
    expect_part("8.8.8.8", true); /* prefilled, the usual check */
    press(OK, 1); /* the cursor starts on Save */
    wait_ms(2300); /* LAN, then WiFi */
    shot("51-ping");
    expect_part("LAN: 4 of 4", true);
    expect_part("WiFi: 4 of 4", true);
    press(BACK, 2);
    wait_ms(300);
    shot("52-home-lan-wifi-icons");
    exit_app();
}

/* Keys as files - generate (with and without passphrase), card, actions, a host on
 * the default key whose passphrase is asked before the connection, a host on a named key,
 * deleting that key (the host switches to the default key), and a key file removed
 * outside the app. Same steps as w_keys in tools/flipper/walks.py. */
static void w_keys(void) {
    wait_ms(600);
    press(DOWN, 4); /* Keys */
    press(OK, 1);
    wait_ms(300);
    shot("k01-keys-empty");
    press(OK, 1); /* Generate key */
    shot("k02-key-type");
    press(OK, 1); /* Ed25519 */
    shot("k03-key-name");
    type("", true, true); /* the suggested id_ed25519 */
    shot("k05-key-passphrase"); /* one prompt, empty for none (as ssh-keygen) */
    type("secret", true, true);
    shot("k06-key-passphrase-again");
    type("secret", true, true);
    wait_ms(300);
    shot("k07-key-generating");
    wait_ms(3000);
    shot("k08-key-card");
    press(RIGHT, 1); /* Actions */
    shot("k09-key-actions");
    press(OK, 1); /* Show public key */
    wait_ms(300);
    shot("k10-public-key");
    press(BACK, 3); /* actions, card, list */
    shot("k11-keys-list");
    press(UP, 1); /* Generate key */
    press(OK, 1);
    press(DOWN, 1); /* ECDSA P-256 */
    press(OK, 1);
    type("", true, true); /* the suggested name */
    type("", true, true); /* empty: no passphrase */
    wait_ms(1500);
    shot("k12-key-card-ecdsa");
    press(BACK, 2); /* list, Home (on Keys) */
    press(UP, 3); /* Hosts */
    press(OK, 1);
    wait_ms(600);
    press(OK, 1); /* nas: the host page, on Connect */
    press(DOWN, 5); /* Auth: Password, Default, id_ecdsa, id_ed25519 */
    press(RIGHT, 1);
    shot("k13-host-auth-default");
    press(DOWN, 5); /* Save (no Save password row for key hosts) */
    press(OK, 1);
    wait_ms(400);
    press(OK, 1); /* the Hosts list */
    press(BACK, 1); /* Home, on Hosts */
    press(DOWN, 3); /* Keys: the cursor is on id_ecdsa, the newest */
    press(OK, 1);
    press(DOWN, 1); /* id_ed25519 */
    press(OK, 1);
    press(RIGHT, 1); /* Actions: Show, Forget passphrase, Rename, Delete */
    press(DOWN, 1);
    press(OK, 1);
    wait_ms(300);
    shot("k14-passphrase-forgotten");
    press(OK, 1);
    press(BACK, 3); /* card, list, Home */
    press(UP, 3); /* Hosts -> nas -> Connect */
    press(OK, 1);
    wait_ms(600);
    press(OK, 1);
    press(OK, 1);
    wait_ms(1200);
    shot("k15-passphrase-prompt");
    type("wrong", true, true);
    shot("k16-remember-first-notice");
    press(RIGHT, 1); /* Remember */
    wait_ms(800);
    shot("k17-passphrase-retry");
    type("secret", true, true);
    shot("k18-remember-passphrase");
    press(RIGHT, 1);
    wait_ms(400);
    shot("k19-unlocking-key");
    wait_ms(3000);
    press(RIGHT, 1); /* trust the new host key */
    wait_ms(1500);
    shot("k20-run-menu-key");
    press(BACK, 1); /* disconnect: back on nas's page, on Connect */
    wait_ms(400);
    expect_text("Connect");
    press(DOWN, 5); /* Auth */
    press(RIGHT, 1); /* id_ecdsa (Password, Default, id_ecdsa, id_ed25519) */
    shot("k21-host-auth-key");
    press(DOWN, 5); /* Save */
    press(OK, 1);
    wait_ms(400);
    press(OK, 1); /* the Hosts list */
    press(BACK, 1); /* Home, on Hosts */
    press(DOWN, 3); /* Keys: the cursor is on id_ed25519 */
    press(OK, 1);
    press(UP, 1); /* id_ecdsa */
    press(OK, 1);
    press(RIGHT, 1); /* Actions: Show, Set as default, Rename, Delete */
    press(UP, 1); /* Delete */
    press(OK, 1);
    wait_ms(300);
    shot("k22-confirm-delete-key");
    press(RIGHT, 1); /* Delete */
    wait_ms(300);
    shot("k23-key-deleted");
    press(OK, 1); /* the Keys list */
    press(BACK, 1); /* Home, on Keys */
    press(UP, 3); /* Hosts -> nas */
    press(OK, 1);
    wait_ms(600);
    press(OK, 1);
    press(DOWN, 5); /* Auth */
    shot("k24-host-auth-switched");
    press(BACK, 1); /* the Hosts list */
    storage_simply_remove(NULL, "/ext/apps_data/ssh_runner/keys/id_ed25519");
    storage_simply_remove(NULL, "/ext/apps_data/ssh_runner/keys/id_ed25519.pub");
    press(OK, 1); /* nas */
    press(OK, 1); /* Connect */
    wait_ms(600);
    shot("k25-connect-key-missing");
    press(RIGHT, 1); /* Edit host */
    wait_ms(400);
    shot("k26-edit-host-from-missing");
    exit_app();
}

/* The keyboard's Shift key (in place of '%'): once for the next key, twice for caps lock,
 * letters drawn in upper case while it is on. */
static void w_kbd_shift(void) {
    wait_ms(600);
    press(OK, 1); /* Quick Connect: the keyboard, on 'q' */
    press(DOWN, 1); /* 'a' */
    press(LEFT, 1); /* wraps to the last key of the row: Shift */
    press(OK, 1); /* once */
    expect_key('Q'); /* the letter keys in upper case */
    press(RIGHT, 1); /* wraps to 'a' */
    press(OK, 2); /* 'A', then 'a': Shift ended with the first */
    expect_key('q');
    press(LEFT, 1);
    press(OK, 2); /* caps lock */
    shot("59b-keyboard-caps-lock");
    press(RIGHT, 1);
    press(OK, 2);
    expect_part("AaAA", true);
    press(LEFT, 1);
    press(OK, 1); /* off */
    press(RIGHT, 1);
    press(OK, 1);
    expect_part("AaAAa", true);
    longp(OK); /* long press still shifts one key */
    expect_part("AaAAaA", true);
    exit_app();
}

/* maxlength: Quick Connect holds user@host:port (102); keys add nothing at the limit, and
 * "used/limit" shows in the header from 80% */
static void w_kbd_limit(void) {
    wait_ms(600);
    press(OK, 1); /* Quick Connect: the keyboard, on 'q' */
    char text[128];
    memset(text, 'a', 81);
    text[81] = 0;
    type(text, false, true);
    expect_part("/102", false); /* 81 of 102: under 80% */
    type("aa", false, false);
    expect_text("83/102");
    shot("59c-keyboard-counter");
    memset(text, 'b', 40);
    text[40] = 0;
    type(text, false, false); /* 19 fit */
    expect_text("102/102");
    expect_part("bbbbbbbbbbbbbbb", true); /* the field shows the end being typed */
    expect_part("Quick Connect", true); /* the header still fits beside the counter */
    shot("59d-keyboard-full");
    exit_app();
}

/* the built-in examples: the FAP's files/examples/, as the device has them in apps_assets */
static const char examples_cmds[] =
    "Filetype: SSH Runner Commands\nVersion: 1\n# Global saved commands, shown for every host\n"
    "Cmd: up=    echo;uptime\nCmd: date=  echo;date '+%F %T'\n";
static const char examples_script[] = "#!/bin/sh\n# sysinfo.sh\nuname -a\n";
#define DATA "/ext/apps_data/ssh_runner"

static void plant_examples(void) {
    fake_fs_mkdir("/assets");
    fake_fs_mkdir("/assets/examples");
    fake_fs_mkdir("/assets/examples/scripts");
    fake_fs_write("/assets/examples/commands.conf", examples_cmds);
    fake_fs_write("/assets/examples/scripts/sysinfo.sh", examples_script);
}

/* settings.conf as it is, without the line that says the examples were offered */
static void forget_seeded(void) {
    char s[1024];
    const char* now = fake_fs_read(DATA "/settings.conf");
    snprintf(s, sizeof(s), "%s", now ? now : "");
    char* line = strstr(s, "Examples: seeded\n");
    if(line) memmove(line, line + 17, strlen(line + 17) + 1);
    fake_fs_write(DATA "/settings.conf", s);
}

/* first run: copied where nothing was, as they are, and recorded */
static void w_examples_first_run(void) {
    wait_ms(300);
    const char* c = fake_fs_read(DATA "/commands.conf");
    if(!c || strcmp(c, examples_cmds))
        FAIL("commands.conf is not the example file: %s", c ? c : "(none)");
    c = fake_fs_read(DATA "/scripts/sysinfo.sh");
    if(!c || strcmp(c, examples_script)) FAIL("sysinfo.sh not copied as it is");
    c = fake_fs_read(DATA "/scripts/backup.sh");
    if(!c || strcmp(c, sample_script)) FAIL("the user's own script changed");
    c = fake_fs_read(DATA "/settings.conf");
    if(!c || !strstr(c, "Examples: seeded")) FAIL("first run not recorded in settings.conf");
    exit_app();
}

/* deleted examples stay deleted; Restore Examples adds what is missing, keeps the user's */
static void w_examples_restore(void) {
    wait_ms(300);
    if(fake_fs_exists(DATA "/scripts/sysinfo.sh")) FAIL("a deleted example came back by itself");
    const char* c = fake_fs_read(DATA "/commands.conf");
    if(!c ||
       strcmp(
           c,
           "Filetype: SSH Runner Commands\nVersion: 1\nCmd: date=  echo;date '+%F %T'\nCmd: mine"))
        FAIL("commands.conf changed at start: %s", c ? c : "(none)");
    press(DOWN, 5);
    press(OK, 1); /* Settings */
    shot("x01-settings-restore-examples");
    press(DOWN, 3);
    press(OK, 1); /* Restore Examples */
    shot("x02-examples-restored");
    expect_part("Added 1 command and", true);
    expect_part("1 script.", true);
    c = fake_fs_read(DATA "/commands.conf");
    if(!c ||
       strcmp(
           c,
           "Filetype: SSH Runner Commands\nVersion: 1\nCmd: date=  echo;date '+%F %T'\nCmd: mine\n"
           "Cmd: up=    echo;uptime\n"))
        FAIL("restore: %s", c ? c : "(none)");
    c = fake_fs_read(DATA "/scripts/sysinfo.sh");
    if(!c || strcmp(c, examples_script)) FAIL("sysinfo.sh not restored");
    press(BACK, 1);
    press(OK, 1); /* again: nothing to add */
    shot("x03-examples-all-there");
    expect_part("All examples are there.", true);
    exit_app();
}

static void w_failure(void) {
    wait_ms(600);
    press(OK, 1);
    shot("56-quick-connect-prefilled");
    type("user@10.0.0.99", true, true);
    wait_ms(2200);
    shot("57-connecting-unreachable");
    wait_ms(1200); /* the mock checks the host after 0.7 s and gives up 2.3 s later */
    shot("58-connect-failed");
    press(LEFT, 1);
    wait_ms(300);
    press(OK, 1);
    press(UP, 2);
    press(LEFT, 3);
    shot("59-keyboard-cursor-edit");
    exit_app();
}

static void w_board(void) {
    wait_ms(600);
    press(DOWN, 5);
    press(OK, 1);
    press(DOWN, 1);
    press(OK, 1);
    wait_ms(500);
    press(DOWN, 4);
    press(OK, 1);
    press(DOWN, 1);
    shot("65-developer-mock-board");
    press(OK, 1);
    wait_ms(600);
    press(BACK, 2);
    wait_ms(500);
    shot("66-home-no-network");
    press(OK, 2);
    press(DOWN, 4);
    press(OK, 1);
    press(DOWN, 1);
    press(OK, 1);
    wait_ms(600);
    press(BACK, 2);
    wait_ms(500);
    shot("67-home-board-gone");
    press(OK, 1);
    press(UP, 1);
    press(OK, 1);
    wait_ms(300);
    press(DOWN, 3);
    shot("68-about-board-gone");
    press(BACK, 1);
    press(DOWN, 1);
    press(OK, 1);
    press(DOWN, 4);
    press(OK, 1);
    press(UP, 2);
    press(OK, 1);
    wait_ms(600);
    press(BACK, 2);
    wait_ms(500);
    shot("69-home-relinked");
    exit_app();
}

/* Settings > Security on the mock: status, the warning, the typed word (OK does nothing
 * until it is "confirm"), progress through the ESP32's restart, the result. */
static void w_security(void) {
    wait_ms(600);
    press(DOWN, 5);
    press(OK, 1); /* Settings */
    press(DOWN, 2);
    press(OK, 1); /* Security */
    wait_ms(400);
    shot("s01-security-plain");
    press(RIGHT, 1); /* Protect */
    shot("s02-protect-confirm");
    press(DOWN, 10); /* to the end of the details */
    shot("s02b-protect-confirm-details");
    press(RIGHT, 1); /* Protect */
    shot("s03-type-confirm");
    type("conf", true, true);
    shot("s04-word-incomplete"); /* still the keyboard: no popup */
    mock_backend_sec_fail_next(true); /* first a failure after the burn */
    type("irm", true, false);
    wait_ms(2000);
    shot("s10-protect-failed");
    press(OK, 1); /* back on Security, still plaintext */
    wait_ms(400);
    press(RIGHT, 1);
    press(RIGHT, 1);
    type("confirm", true, true);
    wait_ms(400);
    shot("s05-protect-writing-key");
    wait_ms(1300);
    shot("s06-protect-encrypting");
    wait_ms(1500);
    shot("s07-protect-restarting");
    wait_ms(2500);
    shot("s08-secrets-protected");
    press(OK, 1);
    wait_ms(400);
    shot("s09-security-protected");
    exit_app();
}

/* Settings > System > Debug off: no Developer entry (stock and community apps) */
/* What the app holds on the heap (host build: pointers are 8 bytes, not 4; the fake
 * SDK's own view objects count too), for RAM budgeting. */
size_t __sanitizer_get_current_allocated_bytes(void); /* ASan */
static size_t mem_base;
static void mem_note(const char* where_) {
    size_t now = __sanitizer_get_current_allocated_bytes();
    printf("render: heap held by the app at %s: %zu B\n", where_, now - mem_base);
}
static void w_memory(void) {
    wait_ms(600);
    mem_note("Home (mock)");
    press(DOWN, 4); /* Keys */
    press(OK, 1);
    wait_ms(300);
    mem_note("Keys");
    press(BACK, 1);
    press(UP, 4);
    press(OK, 1); /* Quick Connect, run a command */
    type("user@192.168.1.10", true, true);
    wait_ms(1000);
    press(RIGHT, 1);
    wait_ms(600);
    type("hunter2", true, true);
    wait_ms(1200);
    press(DOWN, 2);
    press(OK, 1);
    type("seq 3000", true, true);
    wait_ms(4000);
    mem_note("Output (3000 lines)");
    exit_app();
}

static void w_no_debug(void) {
    wait_ms(600);
    press(DOWN, 5);
    press(OK, 1);
    shot("02b-settings-no-debug");
    exit_app();
}

/* The checks themselves must fire: one synthetic frame per rule. */
static Op mk(OpKind k, int x, int y, int w, int h, Color c, Font f, const char* t) {
    Op o = {
        .kind = k,
        .role = RoleOwn,
        .color = c,
        .font = f,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .bx = x,
        .by = y + h};
    snprintf(o.text, sizeof(o.text), "%s", t);
    return o;
}
static void selftest(void) {
    static Frame f;
    struct {
        const char* rule;
        Op a, b;
        uint32_t view;
    } cases[] = {
        {"off screen",
         mk(OpText, 100, 50, 40, 8, ColorBlack, FontSecondary, "wide"),
         mk(OpLine, 0, 0, 1, 1, ColorBlack, 0, ""),
         ViewWidget},
        {"margin",
         mk(OpText, 1, 20, 20, 8, ColorBlack, FontSecondary, "edge"),
         mk(OpLine, 0, 0, 1, 1, ColorBlack, 0, ""),
         ViewWidget},
        {"top margin",
         mk(OpText, 20, 1, 20, 8, ColorBlack, FontPrimary, "Title"),
         mk(OpLine, 0, 63, 1, 1, ColorBlack, 0, ""),
         ViewWidget},
        {"bottom margin",
         mk(OpText, 20, 54, 20, 9, ColorBlack, FontSecondary, "low"),
         mk(OpLine, 0, 0, 1, 1, ColorBlack, 0, ""),
         ViewWidget},
        {"row 63",
         mk(OpText, 20, 56, 20, 8, ColorBlack, FontSecondary, "y"),
         mk(OpLine, 0, 0, 1, 1, ColorBlack, 0, ""),
         ViewHome},
        {"overlap",
         mk(OpText, 10, 20, 30, 8, ColorBlack, FontSecondary, "one"),
         mk(OpText, 30, 24, 30, 8, ColorBlack, FontSecondary, "two"),
         ViewWidget},
        {"black on black",
         mk(OpText, 10, 54, 30, 8, ColorBlack, FontSecondary, "under"),
         mk(OpBox, 0, 52, 128, 12, ColorBlack, 0, ""),
         ViewWidget},
        {"white off box",
         mk(OpText, 10, 20, 30, 8, ColorWhite, FontSecondary, "ghost"),
         mk(OpBox, 0, 52, 128, 12, ColorBlack, 0, ""),
         ViewWidget},
        {"frame",
         mk(OpText, 10, 6, 30, 8, ColorBlack, FontSecondary, "boxed"),
         mk(OpFrame, 1, 8, 126, 12, ColorBlack, 0, ""),
         ViewWidget},
        {"icon",
         mk(OpText, 10, 20, 30, 8, ColorBlack, FontSecondary, "iconic"),
         mk(OpIcon, 20, 20, 10, 10, ColorBlack, 0, "Key"),
         ViewWidget},
        {"title font",
         mk(OpText, 10, 2, 30, 8, ColorBlack, FontSecondary, "Title"),
         mk(OpLine, 0, 63, 1, 1, ColorBlack, 0, ""),
         ViewText},
        {"technical body",
         mk(OpText, 10, 20, 60, 8, ColorBlack, FontSecondary, "Missing user@"),
         mk(OpLine, 0, 63, 1, 1, ColorBlack, 0, ""),
         ViewWidget},
        {"technical title",
         mk(OpText, 10, 3, 80, 8, ColorBlack, FontPrimary, "ESP32 fw 0.1"),
         mk(OpLine, 0, 63, 1, 1, ColorBlack, 0, ""),
         ViewText},
        {"kbd field top",
         mk(OpText, 10, 9, 1, 9, ColorBlack, FontKeyboard, "|"),
         mk(OpLine, 0, 63, 1, 1, ColorBlack, 0, ""),
         ViewKbd},
    };
    snprintf(where, sizeof(where), "selftest");
    for(size_t i = 0; i < COUNT_OF(cases); i++) {
        memset(&f, 0, sizeof(f));
        f.op[0] = cases[i].a;
        f.op[1] = cases[i].b;
        f.n = 2;
        f.view_id = cases[i].view;
        int before = failures;
        FILE* err = stderr;
        stderr = fopen("/dev/null", "w");
        check_frame(&f);
        fclose(stderr);
        stderr = err;
        if(failures == before) {
            fprintf(stderr, "SELFTEST: rule \"%s\" did not fire\n", cases[i].rule);
            exit(1);
        }
        failures = before;
    }
    /* the scrollbar rule: last line in view, thumb still at the top */
    memset(&f, 0, sizeof(f));
    snprintf(tv_src[0], SRC_MAX, "one\ntwo\nthree\nfour");
    f.op[0] = mk(OpText, 2, 14, 20, 7, ColorBlack, FontSecondary, "four");
    f.op[0].by = 21;
    f.op[1] = mk(OpBox, 125, 14, 3, 36, ColorWhite, 0, "");
    f.op[2] = mk(OpBox, 125, 14, 3, 12, ColorBlack, 0, "");
    f.n = 3;
    f.view_id = ViewText;
    int before = failures;
    FILE* err = stderr;
    stderr = fopen("/dev/null", "w");
    check_frame(&f);
    fclose(stderr);
    stderr = err;
    if(failures == before) {
        fprintf(stderr, "SELFTEST: rule \"thumb at the end\" did not fire\n");
        exit(1);
    }
    failures = before;
    tv_src[0][0] = 0;
    frames_checked = 0;
}

/* The app measures FontSecondary without a canvas (core/fontw.c): it must agree with it. */
static void check_fontw(void) {
    const char* s[] = {
        "$ sudo systemctl restart nginx", "Lab-WiFi  100%", "192.168.1.10", "No board", "Wg|", "a"};
    for(size_t i = 0; i < COUNT_OF(s); i++)
        if(fontw_secondary(s[i]) != fake_text_width(FontSecondary, s[i])) {
            fprintf(
                stderr,
                "FAIL fontw \"%s\": %u vs %u\n",
                s[i],
                fontw_secondary(s[i]),
                fake_text_width(FontSecondary, s[i]));
            failures++;
        }
}

int main(void) {
    selftest();
    check_fontw();
    __sanitizer_install_malloc_and_free_hooks(peak_hook, free_hook);
    dump = fopen("render_frames.txt", "w");
    fake_fs_mkdir("/ext/apps_data");
    run("tour", w_tour);
    run("states", w_states);
    run("quick_connect", w_quick_connect);
    run("kbd_shift", w_kbd_shift);
    run("kbd_limit", w_kbd_limit);
    fake_fs_mkdir("/ext/apps_data/ssh_runner/scripts");
    fake_fs_write("/ext/apps_data/ssh_runner/scripts/backup.sh", sample_script);
    run("saved_host", w_saved_host);
    run("dialogs", w_dialogs);
    run("fleet", w_fleet);
    run("network", w_network);
    run("keys", w_keys);
    mem_base = __sanitizer_get_current_allocated_bytes();
    run("memory", w_memory);
    run("failure", w_failure);
    run("board", w_board);
    run("security", w_security);
    plant_examples();
    forget_seeded();
    fake_fs_remove(DATA "/commands.conf");
    run("examples_first_run", w_examples_first_run);
    fake_fs_remove(DATA "/scripts/sysinfo.sh");
    fake_fs_write(
        DATA "/commands.conf",
        "Filetype: SSH Runner Commands\nVersion: 1\nCmd: date=  echo;date '+%F %T'\nCmd: mine");
    run("examples_restore", w_examples_restore);

    /* too little system heap: the stock "Low Memory" dialog, then out, nothing allocated */
    snprintf(where, sizeof(where), "low-memory start");
    fake_set_heap(30000, 30000);
    fake_set_script(NULL);
    fake_app_main();
    if(strncmp(fake_last_dialog(), "Low Memory|", 11)) {
        fprintf(stderr, "FAIL low memory: no stock dialog (got \"%s\")\n", fake_last_dialog());
        failures++;
    }
    fake_set_heap(120000, 60000);
    fake_set_debug(false);
    run("no_debug", w_no_debug);
    fake_set_debug(true);
    if(dump) fclose(dump);
    printf("render: largest peak heap: %zu B (walk %s)\n", walk_peak_max, walk_peak_name);

    snprintf(where, sizeof(where), "coverage");
    for(uint32_t s = 0; s < SshRunnerSceneNum; s++)
        if(!fake_scene_visits(s)) {
            fprintf(stderr, "FAIL scene %s never entered\n", scene_names[s]);
            failures++;
        }
    printf("render: %d frames checked, %d scenes\n", frames_checked, (int)SshRunnerSceneNum);
    printf("render: strings shortened with \"...\" (stock style, for review):\n");
    for(int i = 0; i < n_cut_seen; i++)
        printf("  %s\n", cut_seen[i]);
    printf(
        "render: technical text in a stock module's proportional font (stock Submenu/VariableItemList):\n");
    for(int i = 0; i < n_stock_tech; i++)
        printf("  %s\n", stock_tech_seen[i]);
    printf(
        "render: keyboard, community layout (title on row 0 / touching the field frame, keys on row 63):\n");
    for(int i = 0; i < n_exempt; i++)
        printf("  %s\n", exempt_seen[i]);
    if(failures) {
        printf("render: %d failures\n", failures);
        return 1;
    }
    printf("render: OK\n");
    return 0;
}
