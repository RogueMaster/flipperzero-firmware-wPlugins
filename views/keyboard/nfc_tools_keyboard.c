#include "nfc_tools_keyboard.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

// ── Internal types ─────────────────────────────────────────────────────────

typedef enum {
    KeyboardPageLetters,
    KeyboardPageSymbols,
    KeyboardPageCount,
} KeyboardPage;

struct Keyboard {
    View* view;
};

typedef struct {
    const char    text;
    const uint8_t x; // offset from KB_ORIGIN_X
    const uint8_t y; // offset from KB_ORIGIN_Y
} KbKey;

typedef struct {
    const char*      header;
    char*            text_buffer;
    size_t           text_buffer_size;
    size_t           minimum_length;
    size_t           max_length; // 0 = derive from text_buffer_size
    bool             clear_default_text;
    KeyboardCallback callback;
    void*            callback_context;
    uint8_t          page; // KeyboardPage
    uint8_t          selected_row;
    uint8_t          selected_column;
} KbModel;

static const uint8_t KB_ORIGIN_X  = 1;
static const uint8_t KB_ORIGIN_Y  = 29;
#define KB_ROW_COUNT 3

// Special key sentinels.
#define ENTER_KEY     '\r' // "save" / confirm
#define BACKSPACE_KEY '\b'
#define SWITCH_KEY    '\t' // toggle letters <-> symbols
#define SPACE_KEY     ' '  // dedicated space bar, drawn as a framed "_"

// ── Layout tables ──────────────────────────────

// Letters page — a permanent 0-9 pad sits on the right (0-3 / 4-6 / 7-9).
static const KbKey kb_letters_row0[] = {
    {'q', 1, 8},  {'w', 10, 8}, {'e', 19, 8}, {'r', 28, 8}, {'t', 37, 8},
    {'y', 46, 8}, {'u', 55, 8}, {'i', 64, 8}, {'o', 73, 8}, {'p', 82, 8},
    {'0', 92, 8}, {'1', 102, 8}, {'2', 111, 8}, {'3', 120, 8},
};
static const KbKey kb_letters_row1[] = {
    {'a', 1, 20},  {'s', 10, 20}, {'d', 19, 20}, {'f', 28, 20}, {'g', 37, 20},
    {'h', 46, 20}, {'j', 55, 20}, {'k', 64, 20}, {'l', 73, 20},
    {BACKSPACE_KEY, 82, 11},
    {'4', 102, 20}, {'5', 111, 20}, {'6', 120, 20},
};
static const KbKey kb_letters_row2[] = {
    {SWITCH_KEY, 0, 23},
    {'z', 13, 32}, {'x', 21, 32}, {'c', 29, 32}, {'v', 37, 32}, {'b', 45, 32},
    {'n', 53, 32}, {'m', 61, 32},
    {SPACE_KEY, 69, 23}, // framed space bar (literal '_' lives on the symbols page)
    {ENTER_KEY, 77, 23},
    {'7', 102, 32}, {'8', 111, 32}, {'9', 120, 32},
};

// Symbols page — the ten right-hand keys are the "illegal" symbols so all 32
// ASCII symbols are reachable (digits live on the letters page).
static const KbKey kb_symbols_row0[] = {
    {'!', 2, 8},  {'@', 12, 8}, {'#', 22, 8}, {'$', 32, 8}, {'%', 42, 8},
    {'^', 52, 8}, {'&', 62, 8}, {'(', 71, 8}, {')', 81, 8},
    {'_', 92, 8}, {'<', 102, 8}, {'>', 111, 8}, {':', 120, 8},
};
static const KbKey kb_symbols_row1[] = {
    {'~', 2, 20},  {'+', 12, 20}, {'-', 22, 20}, {'=', 32, 20}, {'[', 42, 20},
    {']', 52, 20}, {'{', 62, 20}, {'}', 72, 20},
    {BACKSPACE_KEY, 82, 11},
    {'"', 102, 20}, {'/', 111, 20}, {'\\', 120, 20},
};
static const KbKey kb_symbols_row2[] = {
    {SWITCH_KEY, 0, 23},
    {'.', 15, 32}, {',', 29, 32}, {';', 41, 32}, {'`', 53, 32}, {'\'', 65, 32},
    {ENTER_KEY, 77, 23},
    {'|', 102, 32}, {'?', 111, 32}, {'*', 120, 32},
};

// ── Layout access ────────────────────────────────────────────────────────────

static const KbKey* kb_page_row(uint8_t page, uint8_t row, uint8_t* out_len) {
    if(page == KeyboardPageSymbols) {
        switch(row) {
        case 0: *out_len = COUNT_OF(kb_symbols_row0); return kb_symbols_row0;
        case 1: *out_len = COUNT_OF(kb_symbols_row1); return kb_symbols_row1;
        case 2: *out_len = COUNT_OF(kb_symbols_row2); return kb_symbols_row2;
        default: furi_crash();
        }
    } else {
        switch(row) {
        case 0: *out_len = COUNT_OF(kb_letters_row0); return kb_letters_row0;
        case 1: *out_len = COUNT_OF(kb_letters_row1); return kb_letters_row1;
        case 2: *out_len = COUNT_OF(kb_letters_row2); return kb_letters_row2;
        default: furi_crash();
        }
    }
}

static uint8_t kb_row_len(uint8_t page, uint8_t row) {
    uint8_t len = 0;
    kb_page_row(page, row, &len);
    return len;
}

static const KbKey* kb_key_at(KbModel* m, uint8_t row, uint8_t col) {
    uint8_t len = 0;
    const KbKey* keys = kb_page_row(m->page, row, &len);
    if(col >= len) col = len - 1;
    return &keys[col];
}

static bool kb_select_key(KbModel* m, char sentinel) {
    for(uint8_t r = 0; r < KB_ROW_COUNT; r++) {
        uint8_t len = 0;
        const KbKey* keys = kb_page_row(m->page, r, &len);
        for(uint8_t c = 0; c < len; c++) {
            if(keys[c].text == sentinel) {
                m->selected_row    = r;
                m->selected_column = c;
                return true;
            }
        }
    }
    return false;
}

// ── Case handling ──────────────────────────────────────────

static bool kb_is_lower(char c) {
    return c >= 'a' && c <= 'z';
}

// Uppercase mapping: lowercase letters shift up; the rest pass through.
static char kb_to_upper(char c) {
    if(kb_is_lower(c)) return (char)(c - 0x20);
    return c;
}

// Whether the *display* shows the upper form (empty field / highlighted default),
// letters page only.
static bool kb_display_upper(KbModel* m) {
    if(m->page != KeyboardPageLetters) return false;
    size_t len = m->text_buffer ? strlen(m->text_buffer) : 0;
    return (len == 0) || m->clear_default_text;
}

// ── Navigation (wrap on both axes; nearest column on vertical moves) ─────────

static void kb_move_horizontal(KbModel* m, int dir) {
    uint8_t len = kb_row_len(m->page, m->selected_row);
    int c = ((int)m->selected_column + dir) % len;
    if(c < 0) c += len;
    m->selected_column = (uint8_t)c;
}

static void kb_move_vertical(KbModel* m, int dir) {
    uint8_t cur_x = kb_key_at(m, m->selected_row, m->selected_column)->x;
    int r = ((int)m->selected_row + dir) % KB_ROW_COUNT;
    if(r < 0) r += KB_ROW_COUNT;

    uint8_t len = kb_row_len(m->page, (uint8_t)r);
    uint8_t best_col = 0;
    uint16_t best_dx = 0xFFFF;
    for(uint8_t c = 0; c < len; c++) {
        uint8_t kx = kb_key_at(m, (uint8_t)r, c)->x;
        uint16_t dx = (kx > cur_x) ? (kx - cur_x) : (cur_x - kx);
        if(dx < best_dx) {
            best_dx  = dx;
            best_col = c;
        }
    }
    m->selected_row    = (uint8_t)r;
    m->selected_column = best_col;
}

// ── Special-key rendering (hand-drawn; SDK icons don't link in a FAP) ─────────

static void kb_draw_backspace(Canvas* canvas, uint8_t x, uint8_t y, bool sel) {
    canvas_set_color(canvas, ColorBlack);
    if(sel) {
        canvas_draw_box(canvas, x, y, 17, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 17, 11, 1);
    }
    uint8_t cx = x + 3, cy = y + 5;
    canvas_draw_line(canvas, cx, cy, cx + 10, cy);
    canvas_draw_line(canvas, cx, cy, cx + 3, cy - 2);
    canvas_draw_line(canvas, cx, cy, cx + 3, cy + 2);
    canvas_draw_line(canvas, cx + 10, cy - 2, cx + 10, cy + 2);
    canvas_set_color(canvas, ColorBlack);
}

static void kb_draw_save(Canvas* canvas, uint8_t x, uint8_t y, bool sel) {
    canvas_set_color(canvas, ColorBlack);
    if(sel) {
        canvas_draw_box(canvas, x, y, 22, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 22, 11, 1);
    }
    canvas_set_font(canvas, FontSecondary);
    uint8_t tw  = canvas_string_width(canvas, "save");
    uint8_t off = (22 > tw) ? (uint8_t)((22 - tw) / 2) : 1;
    canvas_draw_str(canvas, x + off, y + 8, "save");
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorBlack);
}

// Dedicated space bar: a framed button (like save/switch) showing an underscore.
static void kb_draw_space(Canvas* canvas, uint8_t x, uint8_t y, bool sel) {
    canvas_set_color(canvas, ColorBlack);
    if(sel) {
        canvas_draw_box(canvas, x, y, 7, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 7, 11, 1);
    }
    canvas_draw_line(canvas, x + 2, y + 8, x + 4, y + 8); // underscore, ~2px margins
    canvas_set_color(canvas, ColorBlack);
}

// Page toggle:
// with an "A" (top-left) and a "+" (bottom-right). Reproduced pixel-for-pixel
// from KeyKeyboard_10x11.png so it doesn't depend on the SDK icon (which does
// not link in a FAP on Official firmware). Each row is 10 bits, MSB = column 0.
static void kb_draw_switch(Canvas* canvas, uint8_t x, uint8_t y, bool sel) {
    static const uint16_t icon[11] = {
        0x1FE, 0x201, 0x241, 0x2A1, 0x2E1, 0x2A1, 0x209, 0x21D, 0x209, 0x201, 0x1FE};
    static const uint16_t icon_sel[11] = {
        0x1FE, 0x3FF, 0x3BF, 0x35F, 0x31F, 0x35F, 0x3F7, 0x3E3, 0x3F7, 0x3FF, 0x1FE};
    const uint16_t* bmp = sel ? icon_sel : icon;
    canvas_set_color(canvas, ColorBlack);
    for(uint8_t row = 0; row < 11; row++) {
        for(uint8_t col = 0; col < 10; col++) {
            if(bmp[row] & (1u << (9 - col))) canvas_draw_dot(canvas, x + col, y + row);
        }
    }
}

// ── Draw callback ────────────────────────────────────────────────────────────

static void kb_draw_callback(Canvas* canvas, void* _model) {
    KbModel*    model        = _model;
    uint8_t     needed_width = (uint8_t)(canvas_width(canvas) - 8);
    uint8_t     start_pos    = 4;
    const char* text         = model->text_buffer;
    bool        upper        = kb_display_upper(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header (left). No character counter — removed by design.
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, model->header);

    // Text field frame.
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    // Horizontal scroll: drop leading chars until the tail fits, prefix "…".
    if(text && canvas_string_width(canvas, text) > needed_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos    = (uint8_t)(start_pos + 6);
        needed_width = (uint8_t)(needed_width - 8);
    }
    while(text && canvas_string_width(canvas, text) > needed_width) {
        text++;
    }

    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14, canvas_string_width(canvas, text) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 1, 22, "|");
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 2, 22, "|");
    }
    canvas_draw_str(canvas, start_pos, 22, text);
    canvas_set_color(canvas, ColorBlack);

    // Key grid.
    canvas_set_font(canvas, FontKeyboard);
    for(uint8_t row = 0; row < KB_ROW_COUNT; row++) {
        uint8_t      len  = 0;
        const KbKey* keys = kb_page_row(model->page, row, &len);
        for(uint8_t col = 0; col < len; col++) {
            char    key = keys[col].text;
            bool    sel = (model->selected_row == row && model->selected_column == col);
            uint8_t kx  = (uint8_t)(KB_ORIGIN_X + keys[col].x);
            uint8_t ky  = (uint8_t)(KB_ORIGIN_Y + keys[col].y);

            if(key == ENTER_KEY) {
                kb_draw_save(canvas, kx, ky, sel);
            } else if(key == SWITCH_KEY) {
                kb_draw_switch(canvas, kx, ky, sel);
            } else if(key == SPACE_KEY) {
                kb_draw_space(canvas, kx, ky, sel);
            } else if(key == BACKSPACE_KEY) {
                kb_draw_backspace(canvas, kx, ky, sel);
            } else {
                // Printable key: a 9×11 rounded box fits wide glyphs (e.g. "%"), and lowercase / "_" are nudged up 1px.
                if(sel) {
                    canvas_set_color(canvas, ColorBlack);
                    elements_slightly_rounded_box(canvas, kx - 2, ky - 9, 9, 11);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }
                char    glyph = key;
                uint8_t gy    = ky;
                if(upper) {
                    glyph = kb_to_upper(key); // capitals, drawn on the baseline
                } else {
                    gy = (uint8_t)(ky - (kb_is_lower(key) || key == '_'));
                }
                canvas_draw_glyph(canvas, kx, gy, glyph);
            }
        }
    }
}

// ── Editing ──────────────────────────────────────────────────────────────────

static void kb_backspace(KbModel* m) {
    if(!m->text_buffer) return;
    size_t len = m->clear_default_text ? 1 : strlen(m->text_buffer);
    if(len > 0) m->text_buffer[len - 1] = '\0';
    m->clear_default_text = false;
}

static void kb_insert(KbModel* m, char c) {
    if(!m->text_buffer || m->text_buffer_size == 0) return;
    size_t len = m->clear_default_text ? 0 : strlen(m->text_buffer);
    // Hard limit = buffer capacity; a scene-set max_length (if smaller) caps it
    // further (protocol-bounded fields).
    size_t hard_cap = m->text_buffer_size - 1;
    size_t cap = (m->max_length && m->max_length < hard_cap) ? m->max_length : hard_cap;
    if(len < cap) {
        m->text_buffer[len]     = c;
        m->text_buffer[len + 1] = '\0';
    }
    m->clear_default_text = false;
}

static void kb_handle_ok(KbModel* m, bool shift) {
    char selected = kb_key_at(m, m->selected_row, m->selected_column)->text;

    if(selected == SWITCH_KEY) {
        m->page = (uint8_t)((m->page + 1) % KeyboardPageCount);
        uint8_t len = kb_row_len(m->page, m->selected_row);
        if(m->selected_column >= len) m->selected_column = len - 1;
        return;
    }
    if(selected == ENTER_KEY) {
        size_t len = m->text_buffer ? strlen(m->text_buffer) : 0;
        if(m->callback && len >= m->minimum_length) m->callback(m->callback_context);
        return;
    }
    if(selected == BACKSPACE_KEY) {
        kb_backspace(m);
        return;
    }
    if(selected == SPACE_KEY) {
        kb_insert(m, ' ');
        return;
    }

    // Printable key. Uppercase applies when (shift XOR empty-field), letters page
    // only — so the first letter auto-capitalises and long-OK flips the case
    // (and turns '_' into a space).
    size_t text_length = m->text_buffer ? strlen(m->text_buffer) : 0;
    if(m->page == KeyboardPageLetters && (shift != (text_length == 0))) {
        selected = kb_to_upper(selected);
    }
    kb_insert(m, selected);
}

// ── Input callback ───────────────────────────────────────────────────────────

static bool kb_input_callback(InputEvent* event, void* context) {
    Keyboard* kb       = context;
    bool      consumed = false;
    KbModel*  model    = view_get_model(kb->view);

    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            kb_move_vertical(model, -1);
            break;
        case InputKeyDown:
            kb_move_vertical(model, +1);
            break;
        case InputKeyLeft:
            kb_move_horizontal(model, -1);
            break;
        case InputKeyRight:
            kb_move_horizontal(model, +1);
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat)
                kb_handle_ok(model, event->type == InputTypeLong);
            break;
        case InputKeyBack:
            if(event->type == InputTypeLong || event->type == InputTypeRepeat)
                kb_backspace(model);
            else
                consumed = false; // short Back → SceneManager pops the scene
            break;
        default:
            consumed = false;
            break;
        }
    }

    view_commit_model(kb->view, consumed);
    return consumed;
}

// ── Public API ───────────────────────────────────────────────────────────────

Keyboard* keyboard_alloc(void) {
    Keyboard* kb = malloc(sizeof(Keyboard));
    furi_check(kb);
    kb->view = view_alloc();
    view_set_context(kb->view, kb);
    view_allocate_model(kb->view, ViewModelTypeLocking, sizeof(KbModel));
    view_set_draw_callback(kb->view, kb_draw_callback);
    view_set_input_callback(kb->view, kb_input_callback);
    keyboard_reset(kb);
    return kb;
}

void keyboard_free(Keyboard* kb) {
    furi_check(kb);
    view_free(kb->view);
    free(kb);
}

void keyboard_reset(Keyboard* kb) {
    furi_check(kb);
    with_view_model(
        kb->view,
        KbModel * model,
        {
            model->header             = "";
            model->text_buffer        = NULL;
            model->text_buffer_size   = 0;
            model->minimum_length     = 1;
            model->max_length         = 0;
            model->clear_default_text = false;
            model->callback           = NULL;
            model->callback_context   = NULL;
            model->page               = KeyboardPageLetters;
            model->selected_row       = 0;
            model->selected_column    = 0;
        },
        true);
}

View* keyboard_get_view(Keyboard* kb) {
    furi_check(kb);
    return kb->view;
}

void keyboard_set_header_text(Keyboard* kb, const char* text) {
    furi_check(kb);
    with_view_model(kb->view, KbModel * model, { model->header = text; }, true);
}

void keyboard_set_result_callback(
    Keyboard*        kb,
    KeyboardCallback callback,
    void*            callback_context,
    char*            text_buffer,
    size_t           text_buffer_size,
    bool             clear_default_text) {
    furi_check(kb);
    with_view_model(
        kb->view,
        KbModel * model,
        {
            model->callback           = callback;
            model->callback_context   = callback_context;
            model->text_buffer        = text_buffer;
            model->text_buffer_size   = text_buffer_size;
            model->clear_default_text = clear_default_text;
            model->page               = KeyboardPageLetters;

            if(text_buffer && text_buffer[0] != '\0') {
                if(!kb_select_key(model, ENTER_KEY)) {
                    model->selected_row    = 0;
                    model->selected_column = 0;
                }
            } else {
                model->selected_row    = 0;
                model->selected_column = 0;
            }
        },
        true);
}

void keyboard_set_minimum_length(Keyboard* kb, size_t minimum_length) {
    furi_check(kb);
    with_view_model(
        kb->view, KbModel * model, { model->minimum_length = minimum_length; }, true);
}

void keyboard_set_max_length(Keyboard* kb, size_t max_length) {
    furi_check(kb);
    with_view_model(kb->view, KbModel * model, { model->max_length = max_length; }, true);
}
