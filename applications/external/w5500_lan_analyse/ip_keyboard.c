#include "ip_keyboard.h"

#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

/* ── View-model stored inside the View ── */

typedef struct {
    uint8_t octets[4];
    uint8_t prefix; /* CIDR prefix length 0-32 */
    bool cidr_mode;
    uint8_t cursor; /* 0-11 = IP digit, 12-13 = prefix digit (CIDR) */

    const char* header;

    char* result_str;
    uint8_t result_str_size;

    IpKeyboardCallback callback;
    void* callback_context;
} IpKeyboardModel;

struct IpKeyboard {
    View* view;
};

/* ── Helpers ── */

/** Parse "x.x.x.x" or "x.x.x.x/p" into octets[] and prefix. */
static void ip_keyboard_parse(const char* str, uint8_t octets[4], uint8_t* prefix) {
    unsigned int a = 0, b = 0, c = 0, d = 0, p = 24;
    if(!str || !str[0]) {
        memset(octets, 0, 4);
        *prefix = 24;
        return;
    }
    if(sscanf(str, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &p) >= 4) {
        /* OK */
    } else {
        a = b = c = d = 0;
        p = 24;
    }
    octets[0] = (a > 255) ? 255 : (uint8_t)a;
    octets[1] = (b > 255) ? 255 : (uint8_t)b;
    octets[2] = (c > 255) ? 255 : (uint8_t)c;
    octets[3] = (d > 255) ? 255 : (uint8_t)d;
    *prefix = (p > 32) ? 32 : (uint8_t)p;
}

/** Get the digit (0-9) at cursor position. */
static uint8_t ip_kb_get_digit(const IpKeyboardModel* m, uint8_t cursor) {
    if(cursor < 12) {
        uint8_t val = m->octets[cursor / 3];
        uint8_t pos = cursor % 3;
        if(pos == 0) return val / 100;
        if(pos == 1) return (val / 10) % 10;
        return val % 10;
    }
    /* prefix digit */
    if(cursor == 12) return m->prefix / 10;
    return m->prefix % 10;
}

/** Set the digit at cursor position.  Clamps octet to 255, prefix to 32. */
static void ip_kb_set_digit(IpKeyboardModel* m, uint8_t cursor, uint8_t digit) {
    if(digit > 9) digit = 9;

    if(cursor < 12) {
        uint8_t idx = cursor / 3;
        uint8_t pos = cursor % 3;
        uint8_t val = m->octets[idx];
        uint8_t h = val / 100;
        uint8_t t = (val / 10) % 10;
        uint8_t o = val % 10;
        if(pos == 0)
            h = digit;
        else if(pos == 1)
            t = digit;
        else
            o = digit;
        uint16_t nv = (uint16_t)h * 100 + t * 10 + o;
        m->octets[idx] = (nv > 255) ? 255 : (uint8_t)nv;
    } else {
        uint8_t t = m->prefix / 10;
        uint8_t o = m->prefix % 10;
        if(cursor == 12)
            t = digit;
        else
            o = digit;
        uint8_t nv = t * 10 + o;
        m->prefix = (nv > 32) ? 32 : nv;
    }
}

/* ── Draw callback ── */

/*
 * Digit-by-digit IP entry.
 *
 * Display (IP mode):   192 . 168 . 001 . 001
 * Display (CIDR mode): 192.168.001.000 /24
 *
 * The cursor highlights a single digit.  Up/Down cycle 0-9.
 * Left/Right move between digits.  OK confirms.
 */

static void ip_keyboard_draw(Canvas* canvas, void* model) {
    IpKeyboardModel* m = model;
    canvas_clear(canvas);

    /* ── Header ── */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 2, AlignCenter, AlignTop, m->header ? m->header : "Enter IP address:");

    /* ── Measure a single digit cell ── */
    canvas_set_font(canvas, FontPrimary);
    uint8_t cell_w = canvas_string_width(canvas, "0") + 2; /* char + 1px pad each side */
    uint8_t dot_w = canvas_string_width(canvas, ".") + 2;
    uint8_t slash_w = canvas_string_width(canvas, "/") + 2;

    /* Total characters: 12 digits + 3 dots = 15 cells.  CIDR adds /+2 digits. */
    uint16_t total_w = 12 * cell_w + 3 * dot_w;
    if(m->cidr_mode) {
        total_w += slash_w + 2 * cell_w;
    }

    uint8_t sx = (128 > total_w) ? (uint8_t)((128 - total_w) / 2) : 0;
    uint8_t y_base = 36; /* text baseline */
    uint8_t box_top = y_base - 12;
    uint8_t box_h = 15;

    /* Build per-character x-positions and the character to draw */
    /* Max chars: 12 digits + 3 dots + 1 slash + 2 prefix digits = 18 */
    uint8_t char_x[18];
    char char_ch[18];
    int8_t char_cursor[18]; /* cursor index that selects this char, or -1 */
    uint8_t n_chars = 0;

    uint8_t x = sx;
    char digit_buf[2] = {0, 0};

    for(uint8_t oct = 0; oct < 4; oct++) {
        uint8_t val = m->octets[oct];
        uint8_t digits[3] = {val / 100, (val / 10) % 10, val % 10};

        for(uint8_t d = 0; d < 3; d++) {
            char_x[n_chars] = x;
            char_ch[n_chars] = '0' + digits[d];
            char_cursor[n_chars] = (int8_t)(oct * 3 + d);
            n_chars++;
            x += cell_w;
        }

        if(oct < 3) {
            char_x[n_chars] = x;
            char_ch[n_chars] = '.';
            char_cursor[n_chars] = -1;
            n_chars++;
            x += dot_w;
        }
    }

    if(m->cidr_mode) {
        /* Slash */
        char_x[n_chars] = x;
        char_ch[n_chars] = '/';
        char_cursor[n_chars] = -1;
        n_chars++;
        x += slash_w;

        /* Prefix digits (2) */
        uint8_t pd[2] = {m->prefix / 10, m->prefix % 10};
        for(uint8_t d = 0; d < 2; d++) {
            char_x[n_chars] = x;
            char_ch[n_chars] = '0' + pd[d];
            char_cursor[n_chars] = (int8_t)(12 + d);
            n_chars++;
            x += cell_w;
        }
    }

    /* ── Render characters ── */
    for(uint8_t i = 0; i < n_chars; i++) {
        digit_buf[0] = char_ch[i];
        uint8_t sw = canvas_string_width(canvas, digit_buf);
        uint8_t tx = char_x[i] + (cell_w - sw) / 2;
        if(char_ch[i] == '.' || char_ch[i] == '/') {
            uint8_t sep_w = (char_ch[i] == '.') ? dot_w : slash_w;
            tx = char_x[i] + (sep_w - sw) / 2;
        }

        if(char_cursor[i] >= 0 && (uint8_t)char_cursor[i] == m->cursor) {
            /* Selected digit: inverted rounded box */
            canvas_draw_rbox(canvas, char_x[i], box_top, cell_w, box_h, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, tx, y_base, digit_buf);
            canvas_set_color(canvas, ColorBlack);

            /* Up chevron */
            uint8_t cx = char_x[i] + cell_w / 2;
            uint8_t ay = box_top - 5;
            canvas_draw_line(canvas, cx - 3, ay + 3, cx, ay);
            canvas_draw_line(canvas, cx, ay, cx + 3, ay + 3);

            /* Down chevron */
            ay = box_top + box_h + 2;
            canvas_draw_line(canvas, cx - 3, ay, cx, ay + 3);
            canvas_draw_line(canvas, cx, ay + 3, cx + 3, ay);
        } else {
            canvas_draw_str(canvas, tx, y_base, digit_buf);
        }
    }

    /* ── Hint line ── */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "<> move  ^v 0-9  OK send");
}

/* ── Input callback ── */

static bool ip_keyboard_input(InputEvent* event, void* context) {
    IpKeyboard* kb = context;

    if(event->key == InputKeyBack) {
        return false; /* let view_dispatcher navigate back */
    }

    if(event->type != InputTypeShort && event->type != InputTypeLong &&
       event->type != InputTypeRepeat) {
        return false;
    }

    IpKeyboardCallback callback = NULL;
    void* cb_context = NULL;

    with_view_model(
        kb->view,
        IpKeyboardModel * m,
        {
            uint8_t max_cursor = m->cidr_mode ? 13 : 11;

            switch(event->key) {
            case InputKeyUp: {
                uint8_t d = ip_kb_get_digit(m, m->cursor);
                d = (d + 1) % 10;
                ip_kb_set_digit(m, m->cursor, d);
                break;
            }

            case InputKeyDown: {
                uint8_t d = ip_kb_get_digit(m, m->cursor);
                d = (d + 9) % 10; /* -1 mod 10 */
                ip_kb_set_digit(m, m->cursor, d);
                break;
            }

            case InputKeyLeft:
                if(m->cursor > 0) m->cursor--;
                break;

            case InputKeyRight:
                if(m->cursor < max_cursor) m->cursor++;
                break;

            case InputKeyOk:
                if(event->type == InputTypeShort) {
                    if(m->result_str && m->result_str_size > 0) {
                        if(m->cidr_mode) {
                            snprintf(
                                m->result_str,
                                m->result_str_size,
                                "%d.%d.%d.%d/%d",
                                m->octets[0],
                                m->octets[1],
                                m->octets[2],
                                m->octets[3],
                                m->prefix);
                        } else {
                            snprintf(
                                m->result_str,
                                m->result_str_size,
                                "%d.%d.%d.%d",
                                m->octets[0],
                                m->octets[1],
                                m->octets[2],
                                m->octets[3]);
                        }
                    }
                    callback = m->callback;
                    cb_context = m->callback_context;
                }
                break;

            default:
                break;
            }
        },
        true);

    if(callback) {
        callback(cb_context);
    }

    return true;
}

/* ── Public API ── */

IpKeyboard* ip_keyboard_alloc(void) {
    IpKeyboard* kb = malloc(sizeof(IpKeyboard));
    if(!kb) return NULL;
    kb->view = view_alloc();

    view_allocate_model(kb->view, ViewModelTypeLocking, sizeof(IpKeyboardModel));
    view_set_draw_callback(kb->view, ip_keyboard_draw);
    view_set_input_callback(kb->view, ip_keyboard_input);
    view_set_context(kb->view, kb);

    with_view_model(
        kb->view,
        IpKeyboardModel * m,
        {
            m->octets[0] = 192;
            m->octets[1] = 168;
            m->octets[2] = 1;
            m->octets[3] = 1;
            m->prefix = 24;
            m->cidr_mode = false;
            m->cursor = 0;
            m->header = "Enter IP address:";
            m->result_str = NULL;
            m->result_str_size = 0;
            m->callback = NULL;
            m->callback_context = NULL;
        },
        false);

    return kb;
}

void ip_keyboard_free(IpKeyboard* kb) {
    furi_assert(kb);
    view_free(kb->view);
    free(kb);
}

View* ip_keyboard_get_view(IpKeyboard* kb) {
    furi_assert(kb);
    return kb->view;
}

void ip_keyboard_setup(
    IpKeyboard* kb,
    const char* header,
    const char* initial_str,
    bool cidr_mode,
    IpKeyboardCallback callback,
    void* context,
    char* result_buffer,
    uint8_t result_size,
    ViewNavigationCallback back_callback) {
    furi_assert(kb);

    uint8_t octets[4];
    uint8_t prefix;
    ip_keyboard_parse(initial_str, octets, &prefix);

    with_view_model(
        kb->view,
        IpKeyboardModel * m,
        {
            memcpy(m->octets, octets, 4);
            m->prefix = prefix;
            m->cidr_mode = cidr_mode;
            m->cursor = 0;
            m->header = header;
            m->result_str = result_buffer;
            m->result_str_size = result_size;
            m->callback = callback;
            m->callback_context = context;
        },
        true);

    view_set_previous_callback(kb->view, back_callback);
}
