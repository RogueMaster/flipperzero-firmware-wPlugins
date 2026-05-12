#include "nfc_tools_keyboard.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

// ── Internal types ─────────────────────────────────────────────────────────

struct Keyboard {
    View*          view;
    KeyboardLayout layout;
};

typedef struct {
    const char    text; // character value; special sentinels below
    const uint8_t x;   // x offset relative to KB_ORIGIN_X
    const uint8_t y;   // y offset relative to KB_ORIGIN_Y
} KbKey;

typedef struct {
    const char*      header;
    char*            text_buffer;
    size_t           text_buffer_size;
    size_t           minimum_length;
    bool             clear_default_text;
    KeyboardCallback callback;
    void*            callback_context;
    uint8_t          selected_row;
    uint8_t          selected_column;
    uint8_t          page;   // 0 = main (ABC), 1 = SYM — only used by Alpha layout
    KeyboardLayout   layout;
} KbModel;

// ── Constants ──────────────────────────────────────────────────────────────

// Top-left origin of the key grid on the 128x64 Flipper screen.
// Row y-offsets (8, 20, 32) are added to KB_ORIGIN_Y giving actual y values
// of 37, 49, 61 — the glyph baseline within a 64-pixel-high canvas.
static const uint8_t KB_ORIGIN_X  = 1;
static const uint8_t KB_ORIGIN_Y  = 29;
static const uint8_t KB_ROW_COUNT = 3;

// Sentinel characters embedded in the key table to trigger special rendering.
#define ENTER_KEY     '\r'   // confirm / OK button
#define BACKSPACE_KEY '\b'   // delete last character
#define PAGE_KEY      '\x01' // toggle ABC <-> SYM (Alpha layout only)

// ── Shared ABC rows 0 and 1 (identical across all layouts) ────────────────
//
// Row 0 (14 keys): q w e r t y u i o p | 0 1 2 3
// Row 1 (13 keys): a s d f g h j k l ⌫  | 4 5 6

static const KbKey kb_abc_row0[] = {
    {'q', 1, 8},  {'w', 10, 8},  {'e', 19, 8},  {'r', 28, 8},
    {'t', 37, 8}, {'y', 46, 8},  {'u', 55, 8},  {'i', 64, 8},
    {'o', 73, 8}, {'p', 82, 8},
    {'0', 91, 8}, {'1', 100, 8}, {'2', 110, 8}, {'3', 120, 8},
};

static const KbKey kb_abc_row1[] = {
    {'a', 1,  20}, {'s', 10, 20}, {'d', 19, 20}, {'f', 28, 20},
    {'g', 37, 20}, {'h', 46, 20}, {'j', 55, 20}, {'k', 64, 20},
    {'l', 73, 20}, {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20}, {'5', 110, 20}, {'6', 120, 20},
};

// ── Alpha layout — ABC row 2 (12 keys) ───────────────────────────────────
//
// z x c v b n m  [sym 20px]  [ok 12px]  |  7  8  9
// Pixel check: sym ends at (1+64+20)=85, ok ends at (1+86+12)=99,
//              7 starts at (1+100)=101 — 2 px gap. OK.

static const KbKey kb_alpha_abc_row2[] = {
    {'z', 1,  32}, {'x', 10, 32}, {'c', 19, 32}, {'v', 28, 32},
    {'b', 37, 32}, {'n', 46, 32}, {'m', 55, 32},
    {PAGE_KEY,  64, 23},  // 20 px wide button drawn by kb_draw_page()
    {ENTER_KEY, 86, 23},  // 12 px wide button drawn by kb_draw_enter()
    {'7', 100, 32}, {'8', 110, 32}, {'9', 120, 32},
};

// ── Alpha layout — SYM page ───────────────────────────────────────────────
//
// Row 0 (14): ! " # $ % & ' ( ) *  |  0 1 2 3
// Row 1 (13): + - / = < > ; : ^  ⌫  |  4 5 6
// Row 2 (12): { } [ ] \ | ~  [abc]  [ok]  |  , . ?
//   Right col of SYM keeps , . ? (unique to SYM page).
//   Digits 7-9 are on the ABC page right col row 2.

static const KbKey kb_sym_row0[] = {
    {'!', 1,  8}, {'"', 10, 8}, {'#', 19, 8}, {'$', 28, 8},
    {'%', 37, 8}, {'&', 46, 8}, {'\'', 55, 8}, {'(', 64, 8},
    {')', 73, 8}, {'*', 82, 8},
    {'0', 91, 8}, {'1', 100, 8}, {'2', 110, 8}, {'3', 120, 8},
};

static const KbKey kb_sym_row1[] = {
    {'+', 1,  20}, {'-', 10, 20}, {'/', 19, 20}, {'=', 28, 20},
    {'<', 37, 20}, {'>', 46, 20}, {';', 55, 20}, {':', 64, 20},
    {'^', 73, 20}, {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20}, {'5', 110, 20}, {'6', 120, 20},
};

static const KbKey kb_sym_row2[] = {
    {'{', 1,  32}, {'}', 10, 32}, {'[', 19, 32}, {']', 28, 32},
    {'\\', 37, 32}, {'|', 46, 32}, {'~', 55, 32},
    {PAGE_KEY,  64, 23},
    {ENTER_KEY, 86, 23},
    {',', 100, 32}, {'.', 110, 32}, {'?', 120, 32},
};

// ── Email layout — row 2 (13 keys) ───────────────────────────────────────
//
// z x c v b n m  @  .  [ok 12px]  |  7  8  9
// Pixel check: ok ends at (1+83+12)=96, 7 starts at (1+100)=101 — 5 px gap. OK.
// @ and . are directly reachable without a page switch.

static const KbKey kb_email_row2[] = {
    {'z', 1,  32}, {'x', 10, 32}, {'c', 19, 32}, {'v', 28, 32},
    {'b', 37, 32}, {'n', 46, 32}, {'m', 55, 32},
    {'@', 64, 32}, {'.', 73, 32},
    {ENTER_KEY, 83, 23},  // 12 px wide
    {'7', 100, 32}, {'8', 110, 32}, {'9', 120, 32},
};

// ── MIME layout — row 2 (13 keys) ────────────────────────────────────────
//
// z x c v b n m  /  .  [ok 12px]  |  7  8  9
// / and . cover the separator and sub-type of MIME types (e.g. text/plain).

static const KbKey kb_mime_row2[] = {
    {'z', 1,  32}, {'x', 10, 32}, {'c', 19, 32}, {'v', 28, 32},
    {'b', 37, 32}, {'n', 46, 32}, {'m', 55, 32},
    {'/', 64, 32}, {'.', 73, 32},
    {ENTER_KEY, 83, 23},  // 12 px wide
    {'7', 100, 32}, {'8', 110, 32}, {'9', 120, 32},
};

// ── Layout helpers ─────────────────────────────────────────────────────────

// Number of keys in a given row for a given layout and page.
// Row 0: always 14. Row 1: always 13.
// Row 2: 12 for Alpha (both pages), 13 for Email/Mime.
static uint8_t kb_row_size(KeyboardLayout layout, uint8_t page, uint8_t row) {
    if(row == 0) return COUNT_OF(kb_abc_row0);  // 14, same on both pages
    if(row == 1) return COUNT_OF(kb_abc_row1);  // 13, same on both pages
    // row == 2
    if(page == 1)                         return COUNT_OF(kb_sym_row2);        // 12
    if(layout == KeyboardLayoutAlpha)     return COUNT_OF(kb_alpha_abc_row2);  // 12
    if(layout == KeyboardLayoutEmail)     return COUNT_OF(kb_email_row2);      // 13
    if(layout == KeyboardLayoutMime)      return COUNT_OF(kb_mime_row2);       // 13
    furi_crash();
}

// Pointer to the key array for a given row, layout and page.
static const KbKey* kb_get_row(KeyboardLayout layout, uint8_t page, uint8_t row) {
    if(row == 0) return (page == 1) ? kb_sym_row0 : kb_abc_row0;
    if(row == 1) return (page == 1) ? kb_sym_row1 : kb_abc_row1;
    // row == 2
    if(page == 1)                         return kb_sym_row2;
    if(layout == KeyboardLayoutAlpha)     return kb_alpha_abc_row2;
    if(layout == KeyboardLayoutEmail)     return kb_email_row2;
    if(layout == KeyboardLayoutMime)      return kb_mime_row2;
    furi_crash();
}

static char kb_selected_char(KbModel* m) {
    return kb_get_row(m->layout, m->page, m->selected_row)[m->selected_column].text;
}

static bool kb_is_lowercase(char c) {
    return c >= 'a' && c <= 'z';
}

// Return the uppercase equivalent of a character.
// Non-letter characters (digits, @, /, ., symbols) are returned unchanged.
static char kb_to_uppercase(char c) {
    if(kb_is_lowercase(c)) return (char)(c - 0x20);
    return c;
}

// ── Special key drawing ────────────────────────────────────────────────────
// SDK system icons are not exported to FAPs; we render them manually.

// Backspace arrow (16 x 9 px): ←| delete glyph
static void kb_draw_backspace(Canvas* canvas, uint8_t x, uint8_t y, bool sel) {
    canvas_set_color(canvas, ColorBlack);
    if(sel) {
        canvas_draw_box(canvas, x, y, 16, 9);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_line(canvas, x + 2,  y + 4, x + 12, y + 4); // shaft
    canvas_draw_line(canvas, x + 2,  y + 4, x + 5,  y + 2); // arrowhead top
    canvas_draw_line(canvas, x + 2,  y + 4, x + 5,  y + 6); // arrowhead bottom
    canvas_draw_line(canvas, x + 12, y + 2, x + 12, y + 6); // vertical bar
    canvas_set_color(canvas, ColorBlack);
}

// OK button (12 x 11 px): rounded rectangle labelled "ok"
static void kb_draw_enter(Canvas* canvas, uint8_t x, uint8_t y, bool sel) {
    canvas_set_color(canvas, ColorBlack);
    if(sel) {
        canvas_draw_box(canvas, x, y, 12, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 12, 11, 1);
    }
    canvas_set_font(canvas, FontSecondary);
    uint8_t tw  = canvas_string_width(canvas, "ok");
    uint8_t off = (12 > tw) ? (12 - tw) / 2 : 0;
    canvas_draw_str(canvas, x + off, y + 8, "ok");
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorBlack);
}

// SYM / ABC toggle button (20 x 11 px): rounded rectangle with dynamic label.
// The label shows the destination page ("sym" on ABC, "abc" on SYM) so the user
// knows where the button leads, not the current page.
static void kb_draw_page(Canvas* canvas, uint8_t x, uint8_t y, bool sel, uint8_t page) {
    canvas_set_color(canvas, ColorBlack);
    if(sel) {
        canvas_draw_box(canvas, x, y, 20, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 20, 11, 1);
    }
    canvas_set_font(canvas, FontSecondary);
    const char* label = (page == 0) ? "sym" : "abc";
    uint8_t     tw    = canvas_string_width(canvas, label);
    uint8_t     off   = (20 > tw) ? (20 - tw) / 2 : 0;
    canvas_draw_str(canvas, x + off, y + 8, label);
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorBlack);
}

// ── Draw callback ──────────────────────────────────────────────────────────

static void kb_draw_callback(Canvas* canvas, void* _model) {
    KbModel*    model        = _model;
    uint8_t     text_length  = model->text_buffer ? (uint8_t)strlen(model->text_buffer) : 0;
    uint8_t     needed_width = (uint8_t)(canvas_width(canvas) - 8);
    uint8_t     start_pos    = 4;
    const char* text         = model->text_buffer;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header and text field frame
    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    // Scroll the text field if content overflows
    if(text && canvas_string_width(canvas, text) > needed_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos    = (uint8_t)(start_pos + 6);
        needed_width = (uint8_t)(needed_width - 8);
    }
    while(text && canvas_string_width(canvas, text) > needed_width) {
        text++;
    }

    // Render text or selection highlight
    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14,
            canvas_string_width(canvas, text) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        // Cursor: two thin vertical bars after the text
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 1, 22, "|");
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 2, 22, "|");
    }
    canvas_draw_str(canvas, start_pos, 22, text);

    // Render key grid
    canvas_set_font(canvas, FontKeyboard);

    for(uint8_t row = 0; row < KB_ROW_COUNT; row++) {
        uint8_t      col_count = kb_row_size(model->layout, model->page, row);
        const KbKey* keys      = kb_get_row(model->layout, model->page, row);

        for(uint8_t col = 0; col < col_count; col++) {
            char    key_char = keys[col].text;
            bool    sel      = (model->selected_row == row && model->selected_column == col);
            uint8_t kx       = (uint8_t)(KB_ORIGIN_X + keys[col].x);
            uint8_t ky       = (uint8_t)(KB_ORIGIN_Y + keys[col].y);

            if(key_char == ENTER_KEY) {
                kb_draw_enter(canvas, kx, ky, sel);
            } else if(key_char == BACKSPACE_KEY) {
                kb_draw_backspace(canvas, kx, ky, sel);
            } else if(key_char == PAGE_KEY) {
                kb_draw_page(canvas, kx, ky, sel, model->page);
            } else {
                // Regular character — invert colours when selected
                if(sel) {
                    canvas_set_color(canvas, ColorBlack);
                    canvas_draw_box(canvas, kx - 1, ky - 8, 7, 10);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }
                // Show uppercase when the buffer is empty or default is selected
                char glyph = (model->clear_default_text ||
                              (text_length == 0 && kb_is_lowercase(key_char)))
                                 ? kb_to_uppercase(key_char)
                                 : key_char;
                canvas_draw_glyph(canvas, kx, ky, glyph);
            }
        }
    }
}

// ── Input handling ─────────────────────────────────────────────────────────

static void kb_backspace(KbModel* model) {
    // If default text is selected, treat it as a single unit to delete
    uint8_t len = model->clear_default_text ? 1 : (uint8_t)strlen(model->text_buffer);
    if(len > 0) model->text_buffer[len - 1] = '\0';
}

static void kb_handle_ok(KbModel* model, bool shift) {
    char selected = kb_selected_char(model);

    // PAGE_KEY toggles the SYM page (Alpha layout only) — no text modification
    if(selected == PAGE_KEY) {
        model->page ^= 1;
        model->clear_default_text = false;
        return;
    }

    size_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;

    // Uppercase logic: default when buffer is empty or default text is highlighted;
    // long-press (shift) inverts the behaviour.
    bool toggle_case = (text_length == 0 || model->clear_default_text);
    if(shift) toggle_case = !toggle_case;
    if(toggle_case) selected = kb_to_uppercase(selected);

    if(selected == ENTER_KEY) {
        if(model->callback && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
    } else if(selected == BACKSPACE_KEY) {
        kb_backspace(model);
    } else {
        if(model->clear_default_text) text_length = 0;
        if(model->text_buffer && text_length < model->text_buffer_size - 1) {
            model->text_buffer[text_length]     = selected;
            model->text_buffer[text_length + 1] = '\0';
        }
    }
    model->clear_default_text = false;
}

static bool kb_input_callback(InputEvent* event, void* context) {
    Keyboard* kb       = context;
    bool      consumed = false;

    KbModel* model = view_get_model(kb->view);

    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {

        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            if(model->selected_row > 0) {
                model->selected_row--;
                // Clamp column to the new row's bounds
                uint8_t max_col =
                    kb_row_size(model->layout, model->page, model->selected_row) - 1;
                if(model->selected_column > max_col) model->selected_column = max_col;
            }
            break;
        case InputKeyDown:
            if(model->selected_row < KB_ROW_COUNT - 1) {
                model->selected_row++;
                uint8_t max_col =
                    kb_row_size(model->layout, model->page, model->selected_row) - 1;
                if(model->selected_column > max_col) model->selected_column = max_col;
            }
            break;
        case InputKeyLeft:
            if(model->selected_column > 0) {
                model->selected_column--;
            } else {
                // Wrap around to the last key of the current row
                model->selected_column =
                    kb_row_size(model->layout, model->page, model->selected_row) - 1;
            }
            break;
        case InputKeyRight:
            if(model->selected_column <
               kb_row_size(model->layout, model->page, model->selected_row) - 1) {
                model->selected_column++;
            } else {
                model->selected_column = 0; // wrap to start
            }
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat)
                kb_handle_ok(model, event->type == InputTypeLong);
            break;
        case InputKeyBack:
            if(event->type == InputTypeLong || event->type == InputTypeRepeat)
                kb_backspace(model);
            else
                consumed = false; // let SceneManager handle short-Back (pop scene)
            break;
        default:
            consumed = false;
            break;
        }
    }

    view_commit_model(kb->view, consumed);
    return consumed;
}

// ── Public API ─────────────────────────────────────────────────────────────

Keyboard* keyboard_alloc(KeyboardLayout layout) {
    Keyboard* kb = malloc(sizeof(Keyboard));
    furi_check(kb);
    kb->layout = layout;
    kb->view   = view_alloc();
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
            model->layout             = kb->layout;
            model->header             = "";
            model->selected_row       = 0;
            model->selected_column    = 0;
            model->minimum_length     = 1;
            model->clear_default_text = false;
            model->text_buffer        = NULL;
            model->text_buffer_size   = 0;
            model->callback           = NULL;
            model->callback_context   = NULL;
            model->page               = 0; // start on ABC / main page
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
            model->page               = 0; // reset to main page on every new input session

            // If the buffer is pre-filled, position the cursor on the OK button
            // so the user can immediately confirm without navigating.
            // OK is always at (row2_size - 4): right col has 3 digit keys before the
            // end, and OK is the key just before them.
            if(text_buffer && text_buffer[0] != '\0') {
                model->selected_row    = 2;
                model->selected_column =
                    kb_row_size(model->layout, 0, 2) - 4;
            }
        },
        true);
}

void keyboard_set_minimum_length(Keyboard* kb, size_t minimum_length) {
    furi_check(kb);
    with_view_model(
        kb->view,
        KbModel * model,
        { model->minimum_length = minimum_length; },
        true);
}
