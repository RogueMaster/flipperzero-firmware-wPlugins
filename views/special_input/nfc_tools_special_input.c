#include "nfc_tools_special_input.h"
#include <gui/elements.h>
#include <furi.h>

// ── Internal structures ────────────────────────────────────────────────────

struct SpecialInput {
    View* view;
};

typedef struct {
    const char    text;
    const uint8_t x;
    const uint8_t y;
} SpecialInputKey;

typedef struct {
    const char* header;
    char*       text_buffer;
    size_t      text_buffer_size;
    size_t      minimum_length;
    bool        clear_default_text;

    SpecialInputCallback callback;
    void*                callback_context;

    uint8_t selected_row;
    uint8_t selected_column;
    uint8_t page; // 0 = ABC, 1 = SYM
} SpecialInputModel;

// ── Constants ──────────────────────────────────────────────────────────────

static const uint8_t keyboard_origin_x  = 1;
static const uint8_t keyboard_origin_y  = 29;
static const uint8_t keyboard_row_count = 3;

#define ENTER_KEY     '\r'
#define BACKSPACE_KEY '\b'
#define PAGE_KEY      '\x01' // toggle ABC ↔ SYM

// ── ABC layout ────────────────────────────────────────────────────────────
// Row 0 (14): q w e r t y u i o p  |  0 1 2 3
// Row 1 (13): a s d f g h j k l ⌫  |  4 5 6
// Row 2 (12): z x c v b n m [SYM][OK]  |  @ . -

static const SpecialInputKey si_abc_row_0[] = {
    {'q', 1, 8},  {'w', 10, 8},  {'e', 19, 8},  {'r', 28, 8},
    {'t', 37, 8}, {'y', 46, 8},  {'u', 55, 8},  {'i', 64, 8},
    {'o', 73, 8}, {'p', 82, 8},
    {'0', 91, 8}, {'1', 100, 8}, {'2', 110, 8}, {'3', 120, 8},
};

static const SpecialInputKey si_abc_row_1[] = {
    {'a', 1, 20},  {'s', 10, 20}, {'d', 19, 20}, {'f', 28, 20},
    {'g', 37, 20}, {'h', 46, 20}, {'j', 55, 20}, {'k', 64, 20},
    {'l', 73, 20}, {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20}, {'5', 110, 20}, {'6', 120, 20},
};

static const SpecialInputKey si_abc_row_2[] = {
    {'z', 1, 32},  {'x', 10, 32}, {'c', 19, 32}, {'v', 28, 32},
    {'b', 37, 32}, {'n', 46, 32}, {'m', 55, 32},
    {PAGE_KEY,  64, 23},
    {ENTER_KEY, 86, 23},
    {'@', 100, 32}, {'.', 110, 32}, {'-', 120, 32},
};

// ── SYM layout ────────────────────────────────────────────────────────────
// Row 0 (14): ! " # $ % & ' ( ) *  |  0 1 2 3
// Row 1 (13): + - / = < > ; : ^ ⌫  |  4 5 6
// Row 2 (12): { } [ ] \ | ~ [ABC][OK]  |  , . ?

static const SpecialInputKey si_sym_row_0[] = {
    {'!', 1, 8},  {'"', 10, 8},  {'#', 19, 8},  {'$', 28, 8},
    {'%', 37, 8}, {'&', 46, 8},  {'\'', 55, 8}, {'(', 64, 8},
    {')', 73, 8}, {'*', 82, 8},
    {'0', 91, 8}, {'1', 100, 8}, {'2', 110, 8}, {'3', 120, 8},
};

static const SpecialInputKey si_sym_row_1[] = {
    {'+', 1, 20},  {'-', 10, 20}, {'/', 19, 20}, {'=', 28, 20},
    {'<', 37, 20}, {'>', 46, 20}, {';', 55, 20}, {':', 64, 20},
    {'^', 73, 20}, {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20}, {'5', 110, 20}, {'6', 120, 20},
};

static const SpecialInputKey si_sym_row_2[] = {
    {'{', 1, 32},  {'}', 10, 32},  {'[', 19, 32}, {']', 28, 32},
    {'\\', 37, 32}, {'|', 46, 32}, {'~', 55, 32},
    {PAGE_KEY,  64, 23},
    {ENTER_KEY, 86, 23},
    {',', 100, 32}, {'.', 110, 32}, {'?', 120, 32},
};

// ── Navigation helpers ─────────────────────────────────────────────────────

static uint8_t get_row_size(uint8_t row_index) {
    // Identical for both pages
    switch(row_index + 1) {
    case 1:  return COUNT_OF(si_abc_row_0); // 14
    case 2:  return COUNT_OF(si_abc_row_1); // 13
    case 3:  return COUNT_OF(si_abc_row_2); // 12
    default: furi_crash();
    }
}

static const SpecialInputKey* get_row(uint8_t page, uint8_t row_index) {
    if(page == 0) {
        switch(row_index + 1) {
        case 1:  return si_abc_row_0;
        case 2:  return si_abc_row_1;
        case 3:  return si_abc_row_2;
        default: furi_crash();
        }
    } else {
        switch(row_index + 1) {
        case 1:  return si_sym_row_0;
        case 2:  return si_sym_row_1;
        case 3:  return si_sym_row_2;
        default: furi_crash();
        }
    }
}

static char get_selected_char(SpecialInputModel* model) {
    return get_row(model->page, model->selected_row)[model->selected_column].text;
}

static bool char_is_lowercase(char letter) {
    return letter >= 'a' && letter <= 'z';
}

static char char_to_uppercase(char letter) {
    if(char_is_lowercase(letter)) return letter - 0x20;
    return letter; // digits and symbols: unchanged
}

// ── Special keys ───────────────────────────────────────────────────────────

static void draw_backspace_key(Canvas* canvas, uint8_t x, uint8_t y, bool selected) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_box(canvas, x, y, 16, 9);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_line(canvas, x + 2, y + 4, x + 12, y + 4);
    canvas_draw_line(canvas, x + 2, y + 4, x + 5,  y + 2);
    canvas_draw_line(canvas, x + 2, y + 4, x + 5,  y + 6);
    canvas_draw_line(canvas, x + 12, y + 2, x + 12, y + 6);
    canvas_set_color(canvas, ColorBlack);
}

// OK button (12×11) — text centred dynamically
static void draw_enter_key(Canvas* canvas, uint8_t x, uint8_t y, bool selected) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_box(canvas, x, y, 12, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 12, 11, 1);
    }
    canvas_set_font(canvas, FontSecondary);
    uint8_t tw = canvas_string_width(canvas, "ok");
    uint8_t off = (12 > tw) ? (12 - tw) / 2 : 0;
    canvas_draw_str(canvas, x + off, y + 8, "ok");
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorBlack);
}

// SYM / ABC button (20×11) — text centred dynamically.
// canvas_string_width guarantees equal margins regardless of the actual font metrics.
static void draw_page_key(
    Canvas* canvas, uint8_t x, uint8_t y, bool selected, uint8_t page) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_box(canvas, x, y, 20, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 20, 11, 1);
    }
    canvas_set_font(canvas, FontSecondary);
    const char* label = page == 0 ? "sym" : "abc";
    uint8_t tw = canvas_string_width(canvas, label);
    uint8_t off = (20 > tw) ? (20 - tw) / 2 : 0;
    canvas_draw_str(canvas, x + off, y + 8, label);
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorBlack);
}

// ── Rendering ──────────────────────────────────────────────────────────────

static void special_input_view_draw_callback(Canvas* canvas, void* _model) {
    SpecialInputModel* model       = _model;
    uint8_t            text_length = model->text_buffer ? (uint8_t)strlen(model->text_buffer) : 0;
    uint8_t            needed_width = canvas_width(canvas) - 8;
    uint8_t            start_pos   = 4;
    const char*        text        = model->text_buffer;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    if(text && canvas_string_width(canvas, text) > needed_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos    += 6;
        needed_width -= 8;
    }
    while(text != NULL && canvas_string_width(canvas, text) > needed_width) {
        text++;
    }

    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14,
            canvas_string_width(canvas, text) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 1, 22, "|");
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 2, 22, "|");
    }
    canvas_draw_str(canvas, start_pos, 22, text);

    canvas_set_font(canvas, FontKeyboard);

    for(uint8_t row = 0; row < keyboard_row_count; row++) {
        const uint8_t          col_count = get_row_size(row);
        const SpecialInputKey* keys      = get_row(model->page, row);

        for(uint8_t col = 0; col < col_count; col++) {
            char    key_char = keys[col].text;
            bool    sel      = (model->selected_row == row && model->selected_column == col);
            uint8_t kx       = keyboard_origin_x + keys[col].x;
            uint8_t ky       = keyboard_origin_y + keys[col].y;

            if(key_char == ENTER_KEY) {
                draw_enter_key(canvas, kx, ky, sel);
            } else if(key_char == BACKSPACE_KEY) {
                draw_backspace_key(canvas, kx, ky, sel);
            } else if(key_char == PAGE_KEY) {
                draw_page_key(canvas, kx, ky, sel, model->page);
            } else {
                if(sel) {
                    canvas_set_color(canvas, ColorBlack);
                    canvas_draw_box(canvas, kx - 1, ky - 8, 7, 10);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }
                char glyph;
                if(model->clear_default_text ||
                   (text_length == 0 && char_is_lowercase(key_char))) {
                    glyph = char_to_uppercase(key_char);
                } else {
                    glyph = key_char;
                }
                canvas_draw_glyph(canvas, kx, ky, glyph);
            }
        }
    }
}

// ── Input handling ─────────────────────────────────────────────────────────

static void special_input_backspace(SpecialInputModel* model) {
    uint8_t len = model->clear_default_text ? 1 : (uint8_t)strlen(model->text_buffer);
    if(len > 0) model->text_buffer[len - 1] = '\0';
}

static void special_input_handle_ok(SpecialInputModel* model, bool shift) {
    char selected = get_selected_char(model);

    // Page toggle: does not modify the text buffer
    if(selected == PAGE_KEY) {
        model->page ^= 1;
        model->clear_default_text = false;
        return;
    }

    size_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;

    bool toggle_case = (text_length == 0 || model->clear_default_text);
    if(shift) toggle_case = !toggle_case;
    if(toggle_case) selected = char_to_uppercase(selected);

    if(selected == ENTER_KEY) {
        if(model->callback && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
    } else if(selected == BACKSPACE_KEY) {
        special_input_backspace(model);
    } else {
        if(model->clear_default_text) text_length = 0;
        if(model->text_buffer && text_length < model->text_buffer_size - 1) {
            model->text_buffer[text_length]     = selected;
            model->text_buffer[text_length + 1] = '\0';
        }
    }
    model->clear_default_text = false;
}

static bool special_input_view_input_callback(InputEvent* event, void* context) {
    SpecialInput* si       = context;
    bool          consumed = false;

    SpecialInputModel* model = view_get_model(si->view);

    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {

        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            if(model->selected_row > 0) {
                model->selected_row--;
                if(model->selected_column > get_row_size(model->selected_row) - 6)
                    model->selected_column++;
            }
            break;
        case InputKeyDown:
            if(model->selected_row < keyboard_row_count - 1) {
                model->selected_row++;
                if(model->selected_column > get_row_size(model->selected_row) - 4)
                    model->selected_column--;
            }
            break;
        case InputKeyLeft:
            if(model->selected_column > 0) {
                model->selected_column--;
            } else {
                model->selected_column = get_row_size(model->selected_row) - 1;
            }
            break;
        case InputKeyRight:
            if(model->selected_column < get_row_size(model->selected_row) - 1) {
                model->selected_column++;
            } else {
                model->selected_column = 0;
            }
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat)
                special_input_handle_ok(model, event->type == InputTypeLong);
            break;
        case InputKeyBack:
            if(event->type == InputTypeLong || event->type == InputTypeRepeat)
                special_input_backspace(model);
            else
                consumed = false; // let the SceneManager handle the back event
            break;
        default:
            consumed = false;
            break;
        }
    }

    view_commit_model(si->view, consumed);
    return consumed;
}

// ── Public API ─────────────────────────────────────────────────────────────

SpecialInput* special_input_alloc(void) {
    SpecialInput* si = malloc(sizeof(SpecialInput));
    si->view = view_alloc();
    view_set_context(si->view, si);
    view_allocate_model(si->view, ViewModelTypeLocking, sizeof(SpecialInputModel));
    view_set_draw_callback(si->view, special_input_view_draw_callback);
    view_set_input_callback(si->view, special_input_view_input_callback);
    special_input_reset(si);
    return si;
}

void special_input_free(SpecialInput* si) {
    furi_check(si);
    view_free(si->view);
    free(si);
}

void special_input_reset(SpecialInput* si) {
    furi_check(si);
    with_view_model(
        si->view,
        SpecialInputModel * model,
        {
            model->header             = "";
            model->selected_row       = 0;
            model->selected_column    = 0;
            model->minimum_length     = 1;
            model->clear_default_text = false;
            model->text_buffer        = NULL;
            model->text_buffer_size   = 0;
            model->callback           = NULL;
            model->callback_context   = NULL;
            model->page               = 0; // ABC by default
        },
        true);
}

View* special_input_get_view(SpecialInput* si) {
    furi_check(si);
    return si->view;
}

void special_input_set_header_text(SpecialInput* si, const char* text) {
    furi_check(si);
    with_view_model(si->view, SpecialInputModel * model, { model->header = text; }, true);
}

void special_input_set_result_callback(
    SpecialInput*        si,
    SpecialInputCallback callback,
    void*                callback_context,
    char*                text_buffer,
    size_t               text_buffer_size,
    bool                 clear_default_text) {
    furi_check(si);
    with_view_model(
        si->view,
        SpecialInputModel * model,
        {
            model->callback           = callback;
            model->callback_context   = callback_context;
            model->text_buffer        = text_buffer;
            model->text_buffer_size   = text_buffer_size;
            model->clear_default_text = clear_default_text;
            model->page               = 0; // reset to ABC page on each new input
            if(text_buffer && text_buffer[0] != '\0') {
                // Pre-filled → position cursor on OK (row=2, col=8)
                model->selected_row    = 2;
                model->selected_column = 8;
            }
        },
        true);
}

void special_input_set_minimum_length(SpecialInput* si, size_t minimum_length) {
    with_view_model(
        si->view,
        SpecialInputModel * model,
        { model->minimum_length = minimum_length; },
        true);
}
