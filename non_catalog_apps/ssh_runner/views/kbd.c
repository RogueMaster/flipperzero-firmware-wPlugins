/* On-screen keyboard (see kbd.h). SPDX-License-Identifier: GPL-3.0-only
 *
 * Derived from the Flipper firmware's applications/services/gui/modules/text_input.c
 * (Unleashed unlshd-094, GPL-3.0) and the key table of UART Terminal's uart_text_input.c.
 * Drawing, cursor editing, long-press shift and the Save/Backspace images follow the stock
 * module; see THIRD_PARTY.md for sources. */
#include "kbd.h"

#include <furi.h>
#include <gui/elements.h>
#include <ssh_runner_icons.h>

#include "kbd_layout.h"

#define FIELD_X        1
#define FIELD_Y        8
#define FIELD_W        126
#define FIELD_H        12
#define FIELD_BASELINE 17
#define HEADER_Y       7
/* the text cursor: a 1 px line that keeps 1 px from the frame (the FontKeyboard '|' glyph
 * reaches the frame's top row) */
#define CURSOR_TOP     (FIELD_Y + 2)
#define CURSOR_BOTTOM  (FIELD_Y + FIELD_H - 3)

#define REVEAL_MS 1000 /* a masked character shows this long after it is typed */

struct SshKbd {
    View* view;
    FuriTimer* reveal_timer;
    SshKbdCallback cb;
    void* cb_ctx;
    SshKbdValidator validator;
    void* validator_ctx;
};

typedef struct {
    char header[40];
    char hint[32];
    char* text;
    size_t cap;
    size_t cursor; /* insertion point */
    bool cursor_select; /* Up from the top row: Left/Right move the cursor */
    uint8_t row, col;
    bool clear_default;
    bool mask;
    uint8_t shift; /* 0 off, 1 the next key only, 2 caps lock */
    bool reveal; /* mask: the character before the cursor shows until the timer ends */
    size_t min_len;
    size_t max_len; /* 0: the buffer's room */
    char error[64];
} KbdModel;

/* The most characters the field takes: max_len, never more than the buffer holds. */
static size_t limit_of(const KbdModel* m) {
    size_t room = m->cap ? m->cap - 1 : 0;
    return m->max_len && m->max_len < room ? m->max_len : room;
}

static char selected_key(const KbdModel* m) {
    uint8_t n;
    const KbdKey* r = kbd_row(m->row, &n);
    return r[m->col < n ? m->col : n - 1].key;
}

static void draw_field(Canvas* canvas, KbdModel* m) {
    canvas_set_font(canvas, FontSecondary);
    /* from 80% of the limit, "used/limit" at the header's right end, inside the margin */
    uint8_t counter_w = 0;
    size_t lim = limit_of(m), used = m->text ? strlen(m->text) : 0;
    if(lim && used * 5 >= lim * 4) {
        char c[24];
        snprintf(c, sizeof(c), "%u/%u", (unsigned)used, (unsigned)lim);
        canvas_set_font(canvas, FontKeyboard); /* numbers: the technical font */
        counter_w = (uint8_t)canvas_string_width(canvas, c);
        canvas_draw_str(canvas, 126 - counter_w, HEADER_Y, c);
        counter_w += 4; /* a gap before the header's end */
        canvas_set_font(canvas, FontSecondary);
    }
    FuriString* h = furi_string_alloc_set(m->header);
    elements_string_fit_width(
        canvas, h, 124 - counter_w); /* stock ellipsis, inside the 2 px margins */
    canvas_draw_str(canvas, 2, HEADER_Y, furi_string_get_cstr(h));
    furi_string_free(h);
    elements_slightly_rounded_frame(canvas, FIELD_X, FIELD_Y, FIELD_W, FIELD_H);

    canvas_set_font(canvas, FontKeyboard);
    uint8_t cw = canvas_glyph_width(canvas, '0'); /* one cell, 6 px */
    const char* text = m->text ? m->text : "";
    size_t len = strlen(text);
    if(m->cursor > len) m->cursor = len;
    uint8_t x = 4;
    if(!len && m->hint[0] && !m->cursor_select) {
        /* an example after the cursor, in normal ink: "e.g." in words says it is not typed
         * text (a dotted mask reads as garbage); the first key replaces it */
        canvas_draw_line(canvas, x + 2, CURSOR_TOP, x + 2, CURSOR_BOTTOM);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, x + 6, FIELD_BASELINE, "e.g.");
        uint8_t hx = x + 6 + (uint8_t)canvas_string_width(canvas, "e.g.") + 4;
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, hx, FIELD_BASELINE, m->hint);
        return;
    }
    char shown[72];
    /* a value longer than the copy: the part that ends at the cursor (the field shows the
     * cursor's end anyway, with "..." before it) */
    size_t base = 0;
    if(len > sizeof(shown) - 2 && m->cursor > sizeof(shown) - 4)
        base = m->cursor - (sizeof(shown) - 4);
    size_t n = len - base < sizeof(shown) - 2 ? len - base : sizeof(shown) - 2;
    /* masked, but the character just typed shows for REVEAL_MS (as phones do) so its case
     * can be checked */
    for(size_t i = 0; i < n; i++)
        shown[i] = m->mask && !(m->reveal && base + i + 1 == m->cursor) ? '*' : text[base + i];
    shown[n] = 0;
    size_t cur = m->cursor - base < n ? m->cursor - base : n;
    if(m->clear_default) {
        uint16_t w = canvas_string_width(canvas, shown);
        /* one plain rectangle 1 px inside the frame, the text white in it (stock TextInput);
         * ink below the box stays white on the background, so nothing black sticks out
         * under it. '_' sits on the row under the baseline: drawn instead as a short white
         * line on the box's bottom row */
        const uint8_t box_y = FIELD_Y + 2, box_h = FIELD_H - 4;
        elements_slightly_rounded_box(canvas, x - 1, box_y, w + 2, box_h);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, x, FIELD_BASELINE, shown);
        for(size_t i = 0; i < n; i++)
            if(shown[i] == '_')
                canvas_draw_line(
                    canvas, x + i * cw, box_y + box_h - 1, x + i * cw + cw - 2, box_y + box_h - 1);
        canvas_set_color(canvas, ColorBlack);
        return;
    }
    /* insert the cursor, then keep it in view like the stock module; editing the text
     * position (Up from the top row) shows a block cursor instead, unlike a selection */
    memmove(shown + cur + 1, shown + cur, n - cur + 1);
    shown[cur] = ' '; /* the cursor's cell; the cursor is drawn into it below */
    const uint8_t max_w = FIELD_W - 8;
    const char* vis = shown;
    if(canvas_string_width(canvas, vis) > max_w) {
        canvas_draw_str(canvas, x, FIELD_BASELINE, "...");
        uint8_t dots = (uint8_t)canvas_string_width(canvas, "..."); /* 18 px in FontKeyboard */
        x += dots;
        while(vis < shown + cur && canvas_string_width(canvas, vis) > max_w - dots)
            vis++;
    }
    char clip[72];
    snprintf(clip, sizeof(clip), "%s", vis);
    size_t cl = strlen(clip);
    while(cl && canvas_string_width(canvas, clip) > max_w - (x - 4))
        clip[--cl] = 0;
    size_t at = (size_t)((shown + cur) - vis);
    if(m->cursor_select && at < cl) {
        /* the text left and right of a block in the cursor's cell (4 px of a 6 px cell) */
        clip[at] = '\0';
        canvas_draw_str(canvas, x, FIELD_BASELINE, clip);
        canvas_draw_box(canvas, x + (uint8_t)(at * cw) + 1, FIELD_Y + 2, cw - 2, FIELD_H - 4);
        canvas_draw_str(canvas, x + (uint8_t)((at + 1) * cw), FIELD_BASELINE, clip + at + 1);
    } else {
        canvas_draw_str(canvas, x, FIELD_BASELINE, clip);
        if(!m->cursor_select && at < cl) {
            uint8_t cx = x + (uint8_t)(at * cw) + 2; /* where the '|' glyph had its stroke */
            canvas_draw_line(canvas, cx, CURSOR_TOP, cx, CURSOR_BOTTOM);
        }
    }
}

/* An upward arrow, 5 px wide, standing on the baseline like a glyph: outlined when off,
 * filled for the next key, filled with a bar under it for caps lock. */
static void draw_shift(Canvas* canvas, uint8_t x, uint8_t y, uint8_t state) {
    static const char* const arrow[] = {"..#..", ".#.#.", "#...#", ".#.#.", ".#.#.", ".###."};
    static const char* const solid[] = {"..#..", ".###.", "#####", ".###.", ".###.", ".###."};
    const char* const* art = state ? solid : arrow;
    uint8_t top = y - 7;
    for(uint8_t r = 0; r < 6; r++)
        for(uint8_t c = 0; c < 5; c++)
            if(art[r][c] == '#') canvas_draw_dot(canvas, x + c, top + r);
    if(state == 2) canvas_draw_line(canvas, x, y, x + 4, y);
}

static void kbd_draw(Canvas* canvas, void* model_) {
    KbdModel* m = model_;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    draw_field(canvas, m);

    canvas_set_font(canvas, FontKeyboard);
    for(uint8_t row = 0; row < KBD_ROWS; row++) {
        uint8_t n;
        const KbdKey* keys = kbd_row(row, &n);
        for(uint8_t col = 0; col < n; col++) {
            const KbdKey* k = &keys[col];
            bool sel = !m->cursor_select && m->row == row && m->col == col;
            uint8_t x = KBD_ORIGIN_X + k->x, y = KBD_ORIGIN_Y + k->y;
            canvas_set_color(canvas, ColorBlack);
            if(k->key == KBD_ENTER) {
                canvas_draw_icon(canvas, x, y, sel ? &I_KeySaveSelected_24x11 : &I_KeySave_24x11);
            } else if(k->key == KBD_BS) {
                canvas_draw_icon(
                    canvas, x, y, sel ? &I_KeyBackspaceSelected_16x9 : &I_KeyBackspace_16x9);
            } else if(k->key == KBD_SHIFT) {
                if(sel) {
                    KbdRect r = kbd_key_rect(k);
                    canvas_draw_box(canvas, r.x, r.y, r.w, r.h);
                    canvas_set_color(canvas, ColorXOR);
                }
                draw_shift(canvas, x, y, m->shift);
            } else {
                if(sel) {
                    KbdRect r = kbd_key_rect(k);
                    canvas_draw_box(canvas, r.x, r.y, r.w, r.h);
                    /* XOR: rows of tall glyphs ('{', '(', '|') past the box stay visible */
                    canvas_set_color(canvas, ColorXOR);
                }
                /* Shift on: the letters show in upper case, as they will be typed */
                char c = k->key;
                if(m->shift && c >= 'a' && c <= 'z') c = (char)(c - 0x20);
                canvas_draw_glyph(canvas, x, y, c);
            }
        }
    }
    canvas_set_color(canvas, ColorBlack);

    if(m->error[0]) {
        /* the error covers the whole keyboard (no key fragments around it), one 1 px frame;
         * any key dismisses it, like the stock validator message */
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(
            canvas,
            0,
            FIELD_Y + FIELD_H,
            128,
            64 - (FIELD_Y + FIELD_H)); /* tall keys reach row 20 */
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(
            canvas, 2, FIELD_Y + FIELD_H + 3, 124, 64 - (FIELD_Y + FIELD_H + 3) - 2, 3);
        canvas_draw_icon(canvas, 60, 27, &I_Alert_9x8); /* as on every error screen */
        /* first line the message, the line after it an example in FontKeyboard */
        char msg[64];
        snprintf(msg, sizeof(msg), "%s", m->error);
        char* example = strchr(msg, '\n');
        if(example) *example++ = '\0';
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, msg);
        if(example) {
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, example);
        }
    }
}

static void insert_char(KbdModel* m, char c) {
    if(!m->text) return;
    if(m->clear_default) {
        m->text[0] = 0;
        m->cursor = 0;
        m->clear_default = false;
    }
    size_t len = strlen(m->text);
    if(len >= limit_of(m)) return; /* at the field's limit: the key does nothing */
    if(m->cursor > len) m->cursor = len;
    memmove(m->text + m->cursor + 1, m->text + m->cursor, len - m->cursor + 1);
    m->text[m->cursor++] = c;
}

static void backspace(KbdModel* m) {
    if(!m->text) return;
    if(m->clear_default) {
        m->text[0] = 0;
        m->cursor = 0;
        m->clear_default = false;
        return;
    }
    if(m->cursor > 0) {
        char* p = m->text + m->cursor;
        memmove(p - 1, p, strlen(p) + 1);
        m->cursor--;
    }
}

static void move(KbdModel* m, InputKey key) {
    uint8_t n = kbd_row_len(m->row);
    if(m->cursor_select) {
        size_t len = m->text ? strlen(m->text) : 0;
        m->clear_default = false;
        if(key == InputKeyLeft && m->cursor > 0) m->cursor--;
        if(key == InputKeyRight && m->cursor < len) m->cursor++;
        if(key == InputKeyDown) m->cursor_select = false;
        return;
    }
    switch(key) {
    case InputKeyLeft:
        m->col = m->col ? m->col - 1 : n - 1;
        break;
    case InputKeyRight:
        m->col = (m->col + 1 < n) ? m->col + 1 : 0;
        break;
    case InputKeyUp:
        if(m->row > 0) {
            m->col = kbd_nearest(m->row, m->col, m->row - 1);
            m->row--;
        } else {
            m->cursor_select = true; /* stock: edit the text position */
        }
        break;
    case InputKeyDown:
        if(m->row + 1 < KBD_ROWS) {
            m->col = kbd_nearest(m->row, m->col, m->row + 1);
            m->row++;
        }
        break;
    default:
        break;
    }
}

static bool kbd_input(InputEvent* event, void* context) {
    SshKbd* kbd = context;
    bool fire = false, consumed = false, reveal = false;
    with_view_model(
        kbd->view,
        KbdModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeLong) m->reveal = false;
            if(m->error[0] && event->type == InputTypeShort) {
                m->error[0] = 0; /* first key only dismisses the bubble */
                consumed = true;
            } else if(event->key == InputKeyBack) {
                if(event->type == InputTypeLong || event->type == InputTypeRepeat) {
                    backspace(m); /* stock: long Back deletes */
                    consumed = true;
                }
            } else if(
                event->type == InputTypeShort || event->type == InputTypeLong ||
                event->type == InputTypeRepeat) {
                consumed = true;
                if(event->key == InputKeyOk) {
                    if(m->cursor_select) {
                        m->cursor_select = false;
                    } else {
                        char k = selected_key(m);
                        if(k == KBD_ENTER) {
                            if(event->type == InputTypeShort) fire = true;
                        } else if(k == KBD_BS) {
                            if(event->type == InputTypeLong && m->text) {
                                m->text[0] = 0;
                                m->cursor = 0;
                                m->clear_default = false;
                            } else {
                                backspace(m);
                            }
                        } else if(k == KBD_SHIFT) {
                            /* once: the next key; again: caps lock; again: off. Long: lock */
                            if(event->type == InputTypeShort)
                                m->shift = (uint8_t)((m->shift + 1) % 3);
                            else if(event->type == InputTypeLong)
                                m->shift = m->shift == 2 ? 0 : 2;
                        } else if(event->type != InputTypeRepeat) {
                            char c = k;
                            if(event->type == InputTypeLong || m->shift == 1)
                                c = kbd_shift(k);
                            else if(m->shift == 2 && k >= 'a' && k <= 'z')
                                c = (char)(k - 0x20); /* caps lock: letters only */
                            if(m->shift == 1) m->shift = 0;
                            insert_char(m, c);
                            if(m->mask) {
                                m->reveal = true;
                                reveal = true;
                            }
                        }
                    }
                } else {
                    move(m, event->key);
                }
            }
        },
        true);
    if(reveal) furi_timer_start(kbd->reveal_timer, furi_ms_to_ticks(REVEAL_MS));

    if(fire) {
        char err[64] = "";
        bool ok = true;
        size_t min_len = 0;
        char* text = NULL;
        with_view_model(
            kbd->view,
            KbdModel * m,
            {
                min_len = m->min_len;
                text = m->text;
            },
            false);
        if(!text || strlen(text) < min_len) {
            ok = false;
            snprintf(
                err,
                sizeof(err),
                min_len > 1 ? "At least %u characters" : "Enter a value",
                (unsigned)min_len);
        } else if(kbd->validator && !kbd->validator(text, err, sizeof(err), kbd->validator_ctx)) {
            ok = false;
        }
        if(ok) {
            if(kbd->cb) kbd->cb(kbd->cb_ctx);
        } else {
            with_view_model(
                kbd->view,
                KbdModel * m,
                { snprintf(m->error, sizeof(m->error), "%s", err); },
                true);
        }
    }
    return consumed;
}

static void reveal_end(void* context) {
    SshKbd* kbd = context;
    with_view_model(kbd->view, KbdModel * m, { m->reveal = false; }, true);
}

SshKbd* ssh_kbd_alloc(void) {
    SshKbd* kbd = malloc(sizeof(SshKbd));
    memset(kbd, 0, sizeof(*kbd));
    kbd->reveal_timer = furi_timer_alloc(reveal_end, FuriTimerTypeOnce, kbd);
    kbd->view = view_alloc();
    view_allocate_model(kbd->view, ViewModelTypeLocking, sizeof(KbdModel));
    view_set_context(kbd->view, kbd);
    view_set_draw_callback(kbd->view, kbd_draw);
    view_set_input_callback(kbd->view, kbd_input);
    ssh_kbd_reset(kbd);
    return kbd;
}

void ssh_kbd_free(SshKbd* kbd) {
    furi_timer_stop(kbd->reveal_timer);
    furi_timer_free(kbd->reveal_timer);
    view_free(kbd->view);
    free(kbd);
}

View* ssh_kbd_get_view(SshKbd* kbd) {
    return kbd->view;
}

void ssh_kbd_reset(SshKbd* kbd) {
    kbd->cb = NULL;
    kbd->cb_ctx = NULL;
    kbd->validator = NULL;
    kbd->validator_ctx = NULL;
    furi_timer_stop(kbd->reveal_timer);
    with_view_model(kbd->view, KbdModel * m, { memset(m, 0, sizeof(*m)); }, true);
}

void ssh_kbd_set_header(SshKbd* kbd, const char* header) {
    with_view_model(
        kbd->view, KbdModel * m, { snprintf(m->header, sizeof(m->header), "%s", header); }, true);
}

void ssh_kbd_set_hint(SshKbd* kbd, const char* hint) {
    with_view_model(
        kbd->view,
        KbdModel * m,
        { snprintf(m->hint, sizeof(m->hint), "%s", hint ? hint : ""); },
        true);
}

void ssh_kbd_set_result_callback(
    SshKbd* kbd,
    SshKbdCallback callback,
    void* context,
    char* buffer,
    size_t buffer_size,
    bool clear_default) {
    kbd->cb = callback;
    kbd->cb_ctx = context;
    with_view_model(
        kbd->view,
        KbdModel * m,
        {
            m->text = buffer;
            m->cap = buffer_size;
            m->clear_default = clear_default && buffer && buffer[0];
            m->cursor = buffer ? strlen(buffer) : 0;
            m->cursor_select = false;
            m->row = 1; /* on 'q', the stock module's first letter */
            m->col = 0;
            m->error[0] = 0;
        },
        true);
}

void ssh_kbd_select_save(SshKbd* kbd) {
    with_view_model(
        kbd->view,
        KbdModel * m,
        {
            for(uint8_t row = 0; row < KBD_ROWS; row++) {
                uint8_t n;
                const KbdKey* keys = kbd_row(row, &n);
                for(uint8_t col = 0; col < n; col++)
                    if(keys[col].key == KBD_ENTER) {
                        m->row = row;
                        m->col = col;
                    }
            }
        },
        true);
}

void ssh_kbd_set_validator(SshKbd* kbd, SshKbdValidator validator, void* context) {
    kbd->validator = validator;
    kbd->validator_ctx = context;
}

void ssh_kbd_set_mask(SshKbd* kbd, bool mask) {
    with_view_model(kbd->view, KbdModel * m, { m->mask = mask; }, true);
}

void ssh_kbd_set_min_length(SshKbd* kbd, size_t min_length) {
    with_view_model(kbd->view, KbdModel * m, { m->min_len = min_length; }, true);
}

void ssh_kbd_set_max_length(SshKbd* kbd, size_t max_length) {
    with_view_model(kbd->view, KbdModel * m, { m->max_len = max_length; }, true);
}

size_t ssh_kbd_limit(SshKbd* kbd) {
    size_t lim = 0;
    with_view_model(kbd->view, KbdModel * m, { lim = limit_of(m); }, false);
    return lim;
}
