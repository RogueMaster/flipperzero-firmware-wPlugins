#include "text_input.h"
#include "text_input_font_profont11_tf.h"
#include "flippass_icons.h"
#include <gui/elements.h>
#include <furi.h>

struct TextInput {
    View* view;
    FuriTimer* timer;
};

typedef enum {
    TextInputKeyboard_Letters = 0,
    TextInputKeyboard_Special,
    TextInputKeyboard_AccentGrave,
    TextInputKeyboard_AccentAcute,
    TextInputKeyboard_AccentDiaeresis,
    TextInputKeyboard_AccentCircumflex,
    TextInputKeyboard_Count,
} TextInputKeyboard;

typedef enum {
    TextInputKeyActionInsert = 0,
    TextInputKeyActionEnter,
    TextInputKeyActionBackspace,
    TextInputKeyActionSwitchKeyboard,
    TextInputKeyActionAccent,
} TextInputKeyAction;

typedef struct {
    const uint32_t codepoint;
    const uint32_t value;
    const uint8_t x;
    const uint8_t y;
    const uint8_t width;
    const uint8_t height;
    const uint8_t action;
} TextInputKey;

typedef struct {
    const TextInputKey* rows[3];
    const uint8_t keyboard_index;
} Keyboard;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    size_t minimum_length;
    bool clear_default_text;

    TextInputCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;

    TextInputValidatorCallback validator_callback;
    void* validator_callback_context;
    FuriString* validator_text;
    bool validator_message_visible;

    bool illegal_symbols;
    bool cursor_select;
    uint8_t selected_keyboard;
    uint8_t accent_return_row;
    uint8_t accent_return_column;
    size_t cursor_pos;
    bool is_password;
    bool for_open;
} TextInputModel;

static const uint8_t keyboard_origin_x = 1;
static const uint8_t keyboard_origin_y = 29;
static const uint8_t keyboard_row_count = 3;
static const uint8_t text_input_key_box_width = 9;
static const uint8_t text_input_key_box_height = 11;
static const uint8_t text_input_key_box_x_offset = 2;
static const uint8_t text_input_key_box_y_offset = 9;
static const uint8_t text_input_key_glyph_x_visual_offset = 1;
static const uint8_t text_input_default_row = 1;
static const uint8_t text_input_default_column = 5;

#define ENTER_CODEPOINT           '\r'
#define BACKSPACE_CODEPOINT       '\b'
#define SWITCH_KEYBOARD_CODEPOINT '\t'

#define CP_GRAVE_ACCENT             0x0060U
#define CP_ACUTE_ACCENT             0x00B4U
#define CP_DIAERESIS_ACCENT         0x00A8U
#define CP_CIRCUMFLEX_ACCENT        0x005EU
#define CP_C_CEDILLA                0x00E7U
#define CP_N_TILDE                  0x00F1U
#define CP_U_DIAERESIS              0x00FCU
#define CP_SHARP_S                  0x00DFU
#define CP_AE_LIGATURE              0x00E6U
#define CP_OE_LIGATURE              0x0153U
#define CP_O_SLASH                  0x00F8U
#define CP_LATIN_SMALL_A_GRAVE      0x00E0U
#define CP_LATIN_SMALL_E_GRAVE      0x00E8U
#define CP_LATIN_SMALL_I_GRAVE      0x00ECU
#define CP_LATIN_SMALL_O_GRAVE      0x00F2U
#define CP_LATIN_SMALL_U_GRAVE      0x00F9U
#define CP_LATIN_SMALL_Y_GRAVE      0x1EF3U
#define CP_LATIN_SMALL_A_ACUTE      0x00E1U
#define CP_LATIN_SMALL_E_ACUTE      0x00E9U
#define CP_LATIN_SMALL_I_ACUTE      0x00EDU
#define CP_LATIN_SMALL_O_ACUTE      0x00F3U
#define CP_LATIN_SMALL_U_ACUTE      0x00FAU
#define CP_LATIN_SMALL_Y_ACUTE      0x00FDU
#define CP_LATIN_SMALL_A_DIAERESIS  0x00E4U
#define CP_LATIN_SMALL_E_DIAERESIS  0x00EBU
#define CP_LATIN_SMALL_I_DIAERESIS  0x00EFU
#define CP_LATIN_SMALL_O_DIAERESIS  0x00F6U
#define CP_LATIN_SMALL_U_DIAERESIS  0x00FCU
#define CP_LATIN_SMALL_Y_DIAERESIS  0x00FFU
#define CP_LATIN_SMALL_A_CIRCUMFLEX 0x00E2U
#define CP_LATIN_SMALL_E_CIRCUMFLEX 0x00EAU
#define CP_LATIN_SMALL_I_CIRCUMFLEX 0x00EEU
#define CP_LATIN_SMALL_O_CIRCUMFLEX 0x00F4U
#define CP_LATIN_SMALL_U_CIRCUMFLEX 0x00FBU
#define CP_LATIN_SMALL_Y_CIRCUMFLEX 0x0177U

#define KEY_INSERT(cp, x_pos, y_pos)        \
    {                                       \
        .codepoint = (cp),                  \
        .value = (cp),                      \
        .x = (x_pos),                       \
        .y = (y_pos),                       \
        .width = 0,                         \
        .height = 0,                        \
        .action = TextInputKeyActionInsert, \
    }
#define KEY_ENTER(x_pos, y_pos)            \
    {                                      \
        .codepoint = ENTER_CODEPOINT,      \
        .value = 0,                        \
        .x = (x_pos),                      \
        .y = (y_pos),                      \
        .width = 0,                        \
        .height = 0,                       \
        .action = TextInputKeyActionEnter, \
    }
#define KEY_BACKSPACE(x_pos, y_pos)            \
    {                                          \
        .codepoint = BACKSPACE_CODEPOINT,      \
        .value = 0,                            \
        .x = (x_pos),                          \
        .y = (y_pos),                          \
        .width = 0,                            \
        .height = 0,                           \
        .action = TextInputKeyActionBackspace, \
    }
#define KEY_SWITCH(x_pos, y_pos)                    \
    {                                               \
        .codepoint = SWITCH_KEYBOARD_CODEPOINT,     \
        .value = 0,                                 \
        .x = (x_pos),                               \
        .y = (y_pos),                               \
        .width = 0,                                 \
        .height = 0,                                \
        .action = TextInputKeyActionSwitchKeyboard, \
    }
#define KEY_ACCENT(cp, target, x_pos, y_pos) \
    {                                        \
        .codepoint = (cp),                   \
        .value = (target),                   \
        .x = (x_pos),                        \
        .y = (y_pos),                        \
        .width = 0,                          \
        .height = 0,                         \
        .action = TextInputKeyActionAccent,  \
    }

static const TextInputKey keyboard_keys_row_1[] = {
    KEY_INSERT('q', 1, 8),
    KEY_INSERT('w', 10, 8),
    KEY_INSERT('e', 19, 8),
    KEY_INSERT('r', 28, 8),
    KEY_INSERT('t', 37, 8),
    KEY_INSERT('y', 46, 8),
    KEY_INSERT('u', 55, 8),
    KEY_INSERT('i', 64, 8),
    KEY_INSERT('o', 73, 8),
    KEY_INSERT('p', 82, 8),
    KEY_INSERT('0', 92, 8),
    KEY_INSERT('1', 102, 8),
    KEY_INSERT('2', 111, 8),
    KEY_INSERT('3', 120, 8),
};

static const TextInputKey keyboard_keys_row_2[] = {
    KEY_INSERT('a', 1, 20),
    KEY_INSERT('s', 10, 20),
    KEY_INSERT('d', 19, 20),
    KEY_INSERT('f', 28, 20),
    KEY_INSERT('g', 37, 20),
    KEY_INSERT('h', 46, 20),
    KEY_INSERT('j', 55, 20),
    KEY_INSERT('k', 64, 20),
    KEY_INSERT('l', 73, 20),
    KEY_BACKSPACE(82, 11),
    KEY_INSERT('4', 102, 20),
    KEY_INSERT('5', 111, 20),
    KEY_INSERT('6', 120, 20),
};

static const TextInputKey keyboard_keys_row_3[] = {
    KEY_SWITCH(0, 23),
    KEY_INSERT('z', 13, 32),
    KEY_INSERT('x', 21, 32),
    KEY_INSERT('c', 29, 32),
    KEY_INSERT('v', 37, 32),
    KEY_INSERT('b', 45, 32),
    KEY_INSERT('n', 53, 32),
    KEY_INSERT('m', 61, 32),
    KEY_INSERT('_', 69, 32),
    KEY_ENTER(77, 23),
    KEY_INSERT('7', 102, 32),
    KEY_INSERT('8', 111, 32),
    KEY_INSERT('9', 120, 32),
};

static const TextInputKey special_keyboard_keys_row_1[] = {
    KEY_INSERT('!', 2, 8),
    KEY_INSERT('@', 12, 8),
    KEY_INSERT('#', 22, 8),
    KEY_INSERT('$', 32, 8),
    KEY_INSERT('%', 42, 8),
    KEY_ACCENT(CP_CIRCUMFLEX_ACCENT, TextInputKeyboard_AccentCircumflex, 52, 8),
    KEY_INSERT('&', 62, 8),
    KEY_INSERT('(', 71, 8),
    KEY_INSERT(')', 81, 8),
    KEY_INSERT(CP_C_CEDILLA, 92, 8),
    KEY_ACCENT(CP_GRAVE_ACCENT, TextInputKeyboard_AccentGrave, 102, 8),
    KEY_ACCENT(CP_ACUTE_ACCENT, TextInputKeyboard_AccentAcute, 111, 8),
    KEY_ACCENT(CP_DIAERESIS_ACCENT, TextInputKeyboard_AccentDiaeresis, 120, 8),
};

static const TextInputKey special_keyboard_keys_row_2[] = {
    KEY_INSERT('~', 2, 20),
    KEY_INSERT('+', 12, 20),
    KEY_INSERT('-', 22, 20),
    KEY_INSERT('=', 32, 20),
    KEY_INSERT('[', 42, 20),
    KEY_INSERT(']', 52, 20),
    KEY_INSERT('{', 62, 20),
    KEY_INSERT('}', 72, 20),
    KEY_BACKSPACE(82, 11),
    KEY_INSERT(CP_N_TILDE, 102, 20),
    KEY_INSERT(CP_U_DIAERESIS, 111, 20),
    KEY_INSERT(CP_SHARP_S, 120, 20),
};

static const TextInputKey special_keyboard_keys_row_3[] = {
    KEY_SWITCH(0, 23),
    KEY_INSERT('.', 15, 32),
    KEY_INSERT(',', 29, 32),
    KEY_INSERT(';', 41, 32),
    KEY_INSERT('`', 53, 32),
    KEY_INSERT('\'', 65, 32),
    KEY_ENTER(77, 23),
    KEY_INSERT(CP_AE_LIGATURE, 102, 32),
    KEY_INSERT(CP_OE_LIGATURE, 111, 32),
    KEY_INSERT(CP_O_SLASH, 120, 32),
};

static const TextInputKey accent_grave_keys_row_1[] = {
    KEY_INSERT(CP_GRAVE_ACCENT, 1, 8),
    KEY_INSERT(CP_LATIN_SMALL_A_GRAVE, 10, 8),
    KEY_INSERT(CP_LATIN_SMALL_E_GRAVE, 19, 8),
    KEY_INSERT(CP_LATIN_SMALL_I_GRAVE, 28, 8),
    KEY_INSERT(CP_LATIN_SMALL_O_GRAVE, 37, 8),
    KEY_INSERT(CP_LATIN_SMALL_U_GRAVE, 46, 8),
    KEY_INSERT(CP_LATIN_SMALL_Y_GRAVE, 55, 8),
};

static const TextInputKey accent_grave_keys_row_2[] = {
    KEY_BACKSPACE(82, 11),
};

static const TextInputKey accent_grave_keys_row_3[] = {
    KEY_SWITCH(0, 23),
    KEY_ENTER(77, 23),
};

static const TextInputKey accent_acute_keys_row_1[] = {
    KEY_INSERT(CP_ACUTE_ACCENT, 1, 8),
    KEY_INSERT(CP_LATIN_SMALL_A_ACUTE, 10, 8),
    KEY_INSERT(CP_LATIN_SMALL_E_ACUTE, 19, 8),
    KEY_INSERT(CP_LATIN_SMALL_I_ACUTE, 28, 8),
    KEY_INSERT(CP_LATIN_SMALL_O_ACUTE, 37, 8),
    KEY_INSERT(CP_LATIN_SMALL_U_ACUTE, 46, 8),
    KEY_INSERT(CP_LATIN_SMALL_Y_ACUTE, 55, 8),
};

static const TextInputKey accent_acute_keys_row_2[] = {
    KEY_BACKSPACE(82, 11),
};

static const TextInputKey accent_acute_keys_row_3[] = {
    KEY_SWITCH(0, 23),
    KEY_ENTER(77, 23),
};

static const TextInputKey accent_diaeresis_keys_row_1[] = {
    KEY_INSERT(CP_DIAERESIS_ACCENT, 1, 8),
    KEY_INSERT(CP_LATIN_SMALL_A_DIAERESIS, 10, 8),
    KEY_INSERT(CP_LATIN_SMALL_E_DIAERESIS, 19, 8),
    KEY_INSERT(CP_LATIN_SMALL_I_DIAERESIS, 28, 8),
    KEY_INSERT(CP_LATIN_SMALL_O_DIAERESIS, 37, 8),
    KEY_INSERT(CP_U_DIAERESIS, 46, 8),
    KEY_INSERT(CP_LATIN_SMALL_Y_DIAERESIS, 55, 8),
};

static const TextInputKey accent_diaeresis_keys_row_2[] = {
    KEY_BACKSPACE(82, 11),
};

static const TextInputKey accent_diaeresis_keys_row_3[] = {
    KEY_SWITCH(0, 23),
    KEY_ENTER(77, 23),
};

static const TextInputKey accent_circumflex_keys_row_1[] = {
    KEY_INSERT(CP_CIRCUMFLEX_ACCENT, 1, 8),
    KEY_INSERT(CP_LATIN_SMALL_A_CIRCUMFLEX, 10, 8),
    KEY_INSERT(CP_LATIN_SMALL_E_CIRCUMFLEX, 19, 8),
    KEY_INSERT(CP_LATIN_SMALL_I_CIRCUMFLEX, 28, 8),
    KEY_INSERT(CP_LATIN_SMALL_O_CIRCUMFLEX, 37, 8),
    KEY_INSERT(CP_LATIN_SMALL_U_CIRCUMFLEX, 46, 8),
    KEY_INSERT(CP_LATIN_SMALL_Y_CIRCUMFLEX, 55, 8),
};

static const TextInputKey accent_circumflex_keys_row_2[] = {
    KEY_BACKSPACE(82, 11),
};

static const TextInputKey accent_circumflex_keys_row_3[] = {
    KEY_SWITCH(0, 23),
    KEY_ENTER(77, 23),
};

static const Keyboard keyboard_letters = {
    .rows =
        {
            keyboard_keys_row_1,
            keyboard_keys_row_2,
            keyboard_keys_row_3,
        },
    .keyboard_index = TextInputKeyboard_Letters,
};

static const Keyboard keyboard_special = {
    .rows =
        {
            special_keyboard_keys_row_1,
            special_keyboard_keys_row_2,
            special_keyboard_keys_row_3,
        },
    .keyboard_index = TextInputKeyboard_Special,
};

static const Keyboard keyboard_accent_grave = {
    .rows =
        {
            accent_grave_keys_row_1,
            accent_grave_keys_row_2,
            accent_grave_keys_row_3,
        },
    .keyboard_index = TextInputKeyboard_AccentGrave,
};

static const Keyboard keyboard_accent_acute = {
    .rows =
        {
            accent_acute_keys_row_1,
            accent_acute_keys_row_2,
            accent_acute_keys_row_3,
        },
    .keyboard_index = TextInputKeyboard_AccentAcute,
};

static const Keyboard keyboard_accent_diaeresis = {
    .rows =
        {
            accent_diaeresis_keys_row_1,
            accent_diaeresis_keys_row_2,
            accent_diaeresis_keys_row_3,
        },
    .keyboard_index = TextInputKeyboard_AccentDiaeresis,
};

static const Keyboard keyboard_accent_circumflex = {
    .rows =
        {
            accent_circumflex_keys_row_1,
            accent_circumflex_keys_row_2,
            accent_circumflex_keys_row_3,
        },
    .keyboard_index = TextInputKeyboard_AccentCircumflex,
};

static const Keyboard* keyboards[] = {
    &keyboard_letters,
    &keyboard_special,
    &keyboard_accent_grave,
    &keyboard_accent_acute,
    &keyboard_accent_diaeresis,
    &keyboard_accent_circumflex,
};

static const Keyboard* text_input_get_keyboard(TextInputKeyboard keyboard_index) {
    furi_check(keyboard_index < TextInputKeyboard_Count);
    return keyboards[keyboard_index];
}

static void text_input_set_keyboard(TextInputModel* model, TextInputKeyboard keyboard_index) {
    furi_check(model);
    model->selected_keyboard = keyboard_index;
    model->selected_row = 0;
    model->selected_column = 0;
}

static bool text_input_keyboard_is_accent(TextInputKeyboard keyboard_index) {
    return keyboard_index >= TextInputKeyboard_AccentGrave &&
           keyboard_index < TextInputKeyboard_Count;
}

static void switch_keyboard(TextInputModel* model) {
    if(text_input_keyboard_is_accent((TextInputKeyboard)model->selected_keyboard)) {
        text_input_set_keyboard(model, TextInputKeyboard_Special);
    } else if(model->selected_keyboard == TextInputKeyboard_Letters) {
        text_input_set_keyboard(model, TextInputKeyboard_Special);
    } else {
        text_input_set_keyboard(model, TextInputKeyboard_Letters);
    }
}

static bool text_input_utf8_is_continuation_byte(uint8_t byte);
static size_t text_input_utf8_char_size(const char* text);
static size_t text_input_utf8_strlen(const char* text);
static size_t text_input_utf8_prev_boundary(const char* text, size_t byte_pos);
static size_t text_input_utf8_next_boundary(const char* text, size_t byte_pos);
static size_t text_input_utf8_clamp_boundary(const char* text, size_t byte_pos);
static size_t text_input_utf8_encode(uint32_t codepoint, char out[5]);
static void text_input_clamp_selection(TextInputModel* model);
static void text_input_set_utf8_font(Canvas* canvas);
static uint8_t text_input_get_key_box_width(const TextInputKey* key);
static uint8_t text_input_get_key_box_height(const TextInputKey* key);
static int32_t
    text_input_get_key_glyph_x(Canvas* canvas, const TextInputKey* key, uint32_t codepoint);
static void text_input_set_default_selection(TextInputModel* model);
static void text_input_prepare_accent_return(TextInputModel* model);
static void text_input_restore_from_accent(TextInputModel* model);

static void text_input_build_display_text(
    const TextInputModel* model,
    char* buffer,
    size_t buffer_size,
    bool show_real_text) {
    if(buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if(model->text_buffer == NULL) {
        return;
    }

    if(!model->is_password || show_real_text) {
        strlcpy(buffer, model->text_buffer, buffer_size);
        return;
    }

    const size_t text_length = text_input_utf8_strlen(model->text_buffer);
    const size_t masked_length = MIN(text_length, buffer_size - 1U);
    memset(buffer, '*', masked_length);
    buffer[masked_length] = '\0';
}

static void text_input_insert_cursor(char* buffer, size_t cursor_pos, size_t buffer_size) {
    const size_t text_length = strlen(buffer);
    if(cursor_pos > text_length || buffer_size < text_length + 2) {
        return;
    }

    char* move = buffer + cursor_pos;
    memmove(move + 1, move, strlen(move) + 1);
    buffer[cursor_pos] = '|';
}

static void text_input_set_utf8_font(Canvas* canvas) {
    canvas_set_custom_u8g2_font(canvas, text_input_font_profont11_tf);
}

static uint8_t text_input_get_key_box_width(const TextInputKey* key) {
    return key->width ? key->width : text_input_key_box_width;
}

static uint8_t text_input_get_key_box_height(const TextInputKey* key) {
    return key->height ? key->height : text_input_key_box_height;
}

static int32_t
    text_input_get_key_glyph_x(Canvas* canvas, const TextInputKey* key, uint32_t codepoint) {
    const int32_t key_left =
        (int32_t)keyboard_origin_x + key->x - (int32_t)text_input_key_box_x_offset;
    const uint8_t key_width = text_input_get_key_box_width(key);
    const size_t glyph_width = canvas_glyph_width(canvas, (uint16_t)codepoint);

    if(glyph_width >= key_width) {
        return keyboard_origin_x + key->x + text_input_key_glyph_x_visual_offset;
    }

    return key_left + (int32_t)((key_width - glyph_width) / 2U) +
           text_input_key_glyph_x_visual_offset;
}

static void text_input_set_default_selection(TextInputModel* model) {
    model->selected_keyboard = TextInputKeyboard_Letters;
    model->selected_row = text_input_default_row;
    model->selected_column = text_input_default_column;
}

static void text_input_prepare_accent_return(TextInputModel* model) {
    model->accent_return_row = model->selected_row;
    model->accent_return_column = model->selected_column;
}

static void text_input_restore_from_accent(TextInputModel* model) {
    text_input_set_keyboard(model, TextInputKeyboard_Special);
    model->selected_row = model->accent_return_row;
    model->selected_column = model->accent_return_column;
    text_input_clamp_selection(model);
}

static uint8_t get_row_size(const Keyboard* keyboard, uint8_t row_index) {
    uint8_t row_size = 0;

    switch(keyboard->keyboard_index) {
    case TextInputKeyboard_Letters:
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(keyboard_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(keyboard_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(keyboard_keys_row_3);
            break;
        default:
            furi_crash();
        }
        break;
    case TextInputKeyboard_Special:
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(special_keyboard_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(special_keyboard_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(special_keyboard_keys_row_3);
            break;
        default:
            furi_crash();
        }
        break;
    case TextInputKeyboard_AccentGrave:
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(accent_grave_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(accent_grave_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(accent_grave_keys_row_3);
            break;
        default:
            furi_crash();
        }
        break;
    case TextInputKeyboard_AccentAcute:
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(accent_acute_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(accent_acute_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(accent_acute_keys_row_3);
            break;
        default:
            furi_crash();
        }
        break;
    case TextInputKeyboard_AccentDiaeresis:
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(accent_diaeresis_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(accent_diaeresis_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(accent_diaeresis_keys_row_3);
            break;
        default:
            furi_crash();
        }
        break;
    case TextInputKeyboard_AccentCircumflex:
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(accent_circumflex_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(accent_circumflex_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(accent_circumflex_keys_row_3);
            break;
        default:
            furi_crash();
        }
        break;
    default:
        furi_crash();
    }

    return row_size;
}

static const TextInputKey* get_row(const Keyboard* keyboard, uint8_t row_index) {
    const TextInputKey* row = NULL;
    if(row_index < keyboard_row_count) {
        row = keyboard->rows[row_index];
    } else {
        furi_crash();
    }

    return row;
}

static const TextInputKey* get_selected_key(TextInputModel* model) {
    return &get_row(
        text_input_get_keyboard((TextInputKeyboard)model->selected_keyboard),
        model->selected_row)[model->selected_column];
}

static bool text_input_is_ascii_lowercase(uint32_t codepoint) {
    return codepoint >= 0x61U && codepoint <= 0x7AU;
}

static uint32_t text_input_codepoint_to_uppercase(uint32_t codepoint) {
    if(codepoint == '_') {
        return 0x20U;
    }
    if(text_input_is_ascii_lowercase(codepoint)) {
        return codepoint - 0x20U;
    }
    return codepoint;
}

static bool text_input_utf8_is_continuation_byte(uint8_t byte) {
    return (byte & 0xC0U) == 0x80U;
}

static size_t text_input_utf8_char_size(const char* text) {
    const uint8_t byte = (uint8_t)text[0];
    if(byte < 0x80U) {
        return 1U;
    } else if((byte & 0xE0U) == 0xC0U) {
        return 2U;
    } else if((byte & 0xF0U) == 0xE0U) {
        return 3U;
    } else if((byte & 0xF8U) == 0xF0U) {
        return 4U;
    } else {
        return 1U;
    }
}

static size_t text_input_utf8_strlen(const char* text) {
    size_t length = 0;
    if(text == NULL) {
        return 0;
    }
    while(*text != '\0') {
        text += text_input_utf8_char_size(text);
        length++;
    }
    return length;
}

static size_t text_input_utf8_prev_boundary(const char* text, size_t byte_pos) {
    if(text == NULL) {
        return 0;
    }
    if(byte_pos == 0) {
        return 0;
    }
    do {
        byte_pos--;
    } while(byte_pos > 0 && text_input_utf8_is_continuation_byte((uint8_t)text[byte_pos]));
    return byte_pos;
}

static size_t text_input_utf8_next_boundary(const char* text, size_t byte_pos) {
    const size_t text_length = strlen(text);
    if(byte_pos >= text_length) {
        return text_length;
    }
    return MIN(text_length, byte_pos + text_input_utf8_char_size(text + byte_pos));
}

static size_t text_input_utf8_clamp_boundary(const char* text, size_t byte_pos) {
    if(text == NULL) {
        return 0U;
    }
    const size_t text_length = strlen(text);
    if(byte_pos > text_length) {
        byte_pos = text_length;
    }
    while(byte_pos > 0U && text_input_utf8_is_continuation_byte((uint8_t)text[byte_pos])) {
        byte_pos--;
    }
    return byte_pos;
}

static size_t text_input_utf8_encode(uint32_t codepoint, char out[5]) {
    if(codepoint <= 0x7FU) {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return 1U;
    }
    if(codepoint <= 0x7FFU) {
        out[0] = (char)(0xC0U | (codepoint >> 6U));
        out[1] = (char)(0x80U | (codepoint & 0x3FU));
        out[2] = '\0';
        return 2U;
    }
    if(codepoint <= 0xFFFFU) {
        if(codepoint >= 0xD800U && codepoint <= 0xDFFFU) {
            codepoint = '?';
        }
        out[0] = (char)(0xE0U | (codepoint >> 12U));
        out[1] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        out[2] = (char)(0x80U | (codepoint & 0x3FU));
        out[3] = '\0';
        return 3U;
    }
    if(codepoint <= 0x10FFFFU) {
        out[0] = (char)(0xF0U | (codepoint >> 18U));
        out[1] = (char)(0x80U | ((codepoint >> 12U) & 0x3FU));
        out[2] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        out[3] = (char)(0x80U | (codepoint & 0x3FU));
        out[4] = '\0';
        return 4U;
    }

    out[0] = '?';
    out[1] = '\0';
    return 1U;
}

static void text_input_utf8_insert(TextInputModel* model, uint32_t codepoint) {
    if(model->text_buffer == NULL || model->text_buffer_size == 0U) {
        return;
    }

    if(model->clear_default_text) {
        model->text_buffer[0] = '\0';
        model->cursor_pos = 0U;
    }

    const size_t text_length = strlen(model->text_buffer);
    const size_t insert_pos =
        text_input_utf8_clamp_boundary(model->text_buffer, MIN(model->cursor_pos, text_length));
    char encoded[5] = {0};
    const size_t encoded_length = text_input_utf8_encode(codepoint, encoded);
    if(text_length + encoded_length >= model->text_buffer_size) {
        return;
    }

    memmove(
        model->text_buffer + insert_pos + encoded_length,
        model->text_buffer + insert_pos,
        text_length - insert_pos + 1U);
    memcpy(model->text_buffer + insert_pos, encoded, encoded_length);
    model->cursor_pos = insert_pos + encoded_length;
}

static void text_input_backspace_cb(TextInputModel* model) {
    if(model->text_buffer == NULL || model->text_buffer_size == 0U) {
        return;
    }

    if(model->clear_default_text) {
        model->text_buffer[0] = '\0';
        model->cursor_pos = 0U;
        return;
    }

    const size_t text_length = strlen(model->text_buffer);
    if(text_length == 0U || model->cursor_pos == 0U) {
        return;
    }

    const size_t cursor_pos =
        text_input_utf8_clamp_boundary(model->text_buffer, MIN(model->cursor_pos, text_length));
    const size_t delete_pos = text_input_utf8_prev_boundary(model->text_buffer, cursor_pos);
    memmove(
        model->text_buffer + delete_pos,
        model->text_buffer + cursor_pos,
        text_length - cursor_pos + 1U);
    model->cursor_pos = delete_pos;
}

static void text_input_draw_key(
    Canvas* canvas,
    const TextInputModel* model,
    const TextInputKey* key,
    bool selected) {
    const Icon* icon = NULL;

    switch(key->action) {
    case TextInputKeyActionEnter:
        icon = model->for_open ? (selected ? &I_KeyOpenSelected_22x11 : &I_KeyOpen_22x11) :
                                 (selected ? &I_KeySaveSelected_22x11 : &I_KeySave_22x11);
        break;
    case TextInputKeyActionSwitchKeyboard:
        icon = selected ? &I_KeyKeyboardSelected_10x11 : &I_KeyKeyboard_10x11;
        break;
    case TextInputKeyActionBackspace:
        icon = selected ? &I_KeyBackspaceSelected_17x11 : &I_KeyBackspace_17x11;
        break;
    default:
        break;
    }

    canvas_set_color(canvas, ColorBlack);
    if(icon != NULL) {
        canvas_draw_icon(canvas, keyboard_origin_x + key->x, keyboard_origin_y + key->y, icon);
        return;
    }

    if(selected) {
        elements_slightly_rounded_box(
            canvas,
            keyboard_origin_x + key->x - text_input_key_box_x_offset,
            keyboard_origin_y + key->y - text_input_key_box_y_offset,
            text_input_get_key_box_width(key),
            text_input_get_key_box_height(key));
        canvas_set_color(canvas, ColorWhite);
    }

    uint32_t glyph = key->codepoint;
    if(model->selected_keyboard == TextInputKeyboard_Letters &&
       (model->clear_default_text ||
        (model->text_buffer != NULL && model->text_buffer[0] == '\0')) &&
       (text_input_is_ascii_lowercase(key->codepoint) || key->codepoint == '_')) {
        glyph = text_input_codepoint_to_uppercase(key->codepoint);
    }

    canvas_draw_glyph(
        canvas,
        text_input_get_key_glyph_x(canvas, key, glyph),
        keyboard_origin_y + key->y,
        (uint16_t)glyph);
}

static void text_input_view_draw_callback(Canvas* canvas, void* _model) {
    TextInputModel* model = _model;
    const size_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;
    size_t needed_string_width = canvas_width(canvas) - 8;
    size_t start_pos = 4;

    model->cursor_pos = text_input_utf8_clamp_boundary(model->text_buffer, model->cursor_pos);
    const size_t cursor_pos = model->cursor_pos;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    text_input_set_utf8_font(canvas);
    char buf[text_length + 2];
    text_input_build_display_text(model, buf, sizeof(buf), model->cursor_select);
    char* str = buf;

    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14, canvas_string_width(canvas, str) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        text_input_insert_cursor(str, cursor_pos, sizeof(buf));
    }

    if(cursor_pos > 0 && canvas_string_width(canvas, str) > needed_string_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos += 6;
        needed_string_width -= 8;
        size_t off = 0;
        while(str[off] != '\0' && canvas_string_width(canvas, str + off) > needed_string_width &&
              off < cursor_pos) {
            off = text_input_utf8_next_boundary(str, off);
        }
        str += off;
    }

    if(canvas_string_width(canvas, str) > needed_string_width) {
        needed_string_width -= 4;
        size_t len = strlen(str);
        while(len > 0 && canvas_string_width(canvas, str) > needed_string_width) {
            len = text_input_utf8_prev_boundary(str, len);
            str[len] = '\0';
        }
        strlcat(str, "...", sizeof(buf) - (str - buf));
    }

    canvas_draw_str(canvas, start_pos, 22, str);

    text_input_set_utf8_font(canvas);
    const Keyboard* current_keyboard =
        text_input_get_keyboard((TextInputKeyboard)model->selected_keyboard);
    for(uint8_t row = 0; row < keyboard_row_count; row++) {
        const uint8_t column_count = get_row_size(current_keyboard, row);
        const TextInputKey* keys = get_row(current_keyboard, row);

        for(size_t column = 0; column < column_count; column++) {
            const bool selected = !model->cursor_select && model->selected_row == row &&
                                  model->selected_column == column;
            text_input_draw_key(canvas, model, &keys[column], selected);
        }
    }

    if(model->validator_message_visible) {
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 8, 10, 110, 48);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 8, 8, 112, 50, 3);
        canvas_draw_rframe(canvas, 9, 9, 110, 48, 2);
        canvas_draw_str(canvas, 14, 20, "Warning");
        elements_multiline_text(canvas, 14, 34, furi_string_get_cstr(model->validator_text));
        text_input_set_utf8_font(canvas);
    }
}

static void text_input_clamp_selection(TextInputModel* model) {
    const Keyboard* keyboard =
        text_input_get_keyboard((TextInputKeyboard)model->selected_keyboard);
    if(model->selected_row >= keyboard_row_count) {
        model->selected_row = 0;
    }

    const uint8_t row_size = get_row_size(keyboard, model->selected_row);
    if(row_size == 0U) {
        model->selected_column = 0U;
    } else if(model->selected_column >= row_size) {
        model->selected_column = row_size - 1U;
    }
}

static void text_input_handle_up(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->cursor_select = false;
    } else if(model->selected_row > 0) {
        model->selected_row--;
        text_input_clamp_selection(model);
    } else {
        model->cursor_select = true;
        model->clear_default_text = false;
    }
}

static void text_input_handle_down(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->cursor_select = false;
    } else if(model->selected_row < keyboard_row_count - 1) {
        model->selected_row++;
        text_input_clamp_selection(model);
    }
}

static void text_input_handle_left(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->clear_default_text = false;
        if(model->text_buffer != NULL && model->cursor_pos > 0U) {
            model->cursor_pos =
                text_input_utf8_prev_boundary(model->text_buffer, model->cursor_pos);
        }
    } else if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        model->selected_column =
            get_row_size(
                text_input_get_keyboard((TextInputKeyboard)model->selected_keyboard),
                model->selected_row) -
            1U;
    }
}

static void text_input_handle_right(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->clear_default_text = false;
        if(model->text_buffer != NULL) {
            model->cursor_pos =
                text_input_utf8_next_boundary(model->text_buffer, model->cursor_pos);
        }
    } else if(
        model->selected_column <
        get_row_size(
            text_input_get_keyboard((TextInputKeyboard)model->selected_keyboard),
            model->selected_row) -
            1U) {
        model->selected_column++;
    } else {
        model->selected_column = 0;
    }
}

static void text_input_handle_ok(TextInput* text_input, TextInputModel* model, InputType type) {
    if(model->cursor_select) {
        model->clear_default_text = !model->clear_default_text;
        return;
    }

    const TextInputKey* selected_key = get_selected_key(model);
    const bool shift = type == InputTypeLong;
    const bool repeat = type == InputTypeRepeat;
    const size_t text_length = model->text_buffer ? text_input_utf8_strlen(model->text_buffer) :
                                                    0U;

    switch((TextInputKeyAction)selected_key->action) {
    case TextInputKeyActionEnter:
        if(model->validator_callback &&
           (!model->validator_callback(
               model->text_buffer, model->validator_text, model->validator_callback_context))) {
            model->validator_message_visible = true;
            furi_timer_start(text_input->timer, furi_kernel_get_tick_frequency() * 4);
        } else if(model->callback != 0 && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
        break;
    case TextInputKeyActionSwitchKeyboard:
        switch_keyboard(model);
        break;
    case TextInputKeyActionBackspace:
        if(!repeat) {
            text_input_backspace_cb(model);
        }
        break;
    case TextInputKeyActionAccent:
        text_input_prepare_accent_return(model);
        text_input_set_keyboard(model, (TextInputKeyboard)selected_key->value);
        break;
    case TextInputKeyActionInsert: {
        uint32_t codepoint = selected_key->codepoint;
        const bool restore_to_special =
            text_input_keyboard_is_accent((TextInputKeyboard)model->selected_keyboard);
        const size_t effective_length = model->clear_default_text ? 0U : text_length;
        if(model->selected_keyboard == TextInputKeyboard_Letters &&
           (shift != (effective_length == 0U))) {
            codepoint = text_input_codepoint_to_uppercase(codepoint);
        }
        if(!repeat) {
            text_input_utf8_insert(model, codepoint);
            if(restore_to_special) {
                text_input_restore_from_accent(model);
            }
        }
        break;
    }
    default:
        break;
    }

    model->clear_default_text = false;
}

static bool text_input_view_input_callback(InputEvent* event, void* context) {
    TextInput* text_input = context;
    furi_assert(text_input);

    bool consumed = false;

    // Acquire model
    TextInputModel* model = view_get_model(text_input->view);

    if((!(event->type == InputTypePress) && !(event->type == InputTypeRelease)) &&
       model->validator_message_visible) {
        model->validator_message_visible = false;
        consumed = true;
    } else if(event->type == InputTypeShort) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            text_input_handle_up(text_input, model);
            break;
        case InputKeyDown:
            text_input_handle_down(text_input, model);
            break;
        case InputKeyLeft:
            text_input_handle_left(text_input, model);
            break;
        case InputKeyRight:
            text_input_handle_right(text_input, model);
            break;
        case InputKeyOk:
            text_input_handle_ok(text_input, model, event->type);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeLong) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            text_input_handle_up(text_input, model);
            break;
        case InputKeyDown:
            text_input_handle_down(text_input, model);
            break;
        case InputKeyLeft:
            text_input_handle_left(text_input, model);
            break;
        case InputKeyRight:
            text_input_handle_right(text_input, model);
            break;
        case InputKeyOk:
            text_input_handle_ok(text_input, model, event->type);
            break;
        case InputKeyBack:
            text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            text_input_handle_up(text_input, model);
            break;
        case InputKeyDown:
            text_input_handle_down(text_input, model);
            break;
        case InputKeyLeft:
            text_input_handle_left(text_input, model);
            break;
        case InputKeyRight:
            text_input_handle_right(text_input, model);
            break;
        case InputKeyOk:
            text_input_handle_ok(text_input, model, event->type);
            break;
        case InputKeyBack:
            text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    // Commit model
    view_commit_model(text_input->view, consumed);

    return consumed;
}

/*static bool text_input_view_ascii_callback(AsciiEvent* event, void* context) {
    TextInput* text_input = context;
    furi_assert(text_input);

    switch(event->value) {
    case AsciiValueDC3: // Right
    case AsciiValueDC4: // Left
        with_view_model(
            text_input->view,
            TextInputModel * model,
            {
                model->cursor_select = true;
                model->clear_default_text = false;
                model->selected_row = 0;
                if(event->value == AsciiValueDC3) {
                    model->cursor_pos =
                        CLAMP(model->cursor_pos + 1, strlen(model->text_buffer), 0u);
                } else {
                    if(model->cursor_pos > 0) {
                        model->cursor_pos =
                            CLAMP(model->cursor_pos - 1, strlen(model->text_buffer), 0u);
                    }
                }
            },
            true);
        return true;
    case _AsciiValueSOH: // Ctrl A
        with_view_model(
            text_input->view,
            TextInputModel * model,
            { model->clear_default_text = !model->clear_default_text; },
            true);
        return true;
    default: // Look in keyboards
        TextInputModel* model = view_get_model(text_input->view);
        uint8_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;
        bool uppercase = model->clear_default_text || text_length == 0;
        for(size_t k = 0; k < keyboard_count; k++) {
            bool symbols = k == symbol_keyboard.keyboard_index;
            const Keyboard* keyboard = keyboards[k];
            for(size_t r = 0; r < keyboard_row_count; r++) {
                const TextInputKey* row = get_row(keyboard, r);
                uint8_t size = get_row_size(keyboard, r);
                for(size_t key = 0; key < size; key++) {
                    char lower = row[key].text;
                    if(symbols && model->illegal_symbols) lower = char_to_illegal_symbol(lower);
                    char upper = symbols ? lower : char_to_uppercase(lower);
                    if(event->value == lower || event->value == upper) {
                        model->cursor_select = false;
                        model->selected_keyboard = k;
                        model->selected_row = r;
                        model->selected_column = key;
                        bool shift = (event->value == upper) != uppercase && !symbols;
                        text_input_handle_ok(
                            text_input, model, shift ? InputTypeLong : InputTypeShort);
                        view_commit_model(text_input->view, true);
                        return true;
                    }
                }
            }
        }
        view_commit_model(text_input->view, false);
        break;
    }

    return false;
}*/

void text_input_timer_callback(void* context) {
    furi_assert(context);
    TextInput* text_input = context;

    with_view_model(
        text_input->view,
        TextInputModel * model,
        { model->validator_message_visible = false; },
        true);
}

TextInput* text_input_alloc(void) {
    TextInput* text_input = malloc(sizeof(TextInput));
    text_input->view = view_alloc();
    view_set_context(text_input->view, text_input);
    view_allocate_model(text_input->view, ViewModelTypeLocking, sizeof(TextInputModel));
    view_set_draw_callback(text_input->view, text_input_view_draw_callback);
    view_set_input_callback(text_input->view, text_input_view_input_callback);
    //view_set_ascii_callback(text_input->view, text_input_view_ascii_callback);

    text_input->timer = furi_timer_alloc(text_input_timer_callback, FuriTimerTypeOnce, text_input);

    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->validator_text = furi_string_alloc();
            model->minimum_length = 1;
            model->illegal_symbols = false;
            model->cursor_pos = 0;
            model->cursor_select = false;
            model->accent_return_row = 0;
            model->accent_return_column = 0;
            model->is_password = false;
            model->for_open = false;
        },
        false);

    text_input_reset(text_input);

    return text_input;
}

void text_input_free(TextInput* text_input) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { furi_string_free(model->validator_text); },
        false);

    // Send stop command
    furi_timer_stop(text_input->timer);
    // Release allocated memory
    furi_timer_free(text_input->timer);

    view_free(text_input->view);

    free(text_input);
}

void text_input_reset(TextInput* text_input) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->selected_keyboard = 0;
            model->minimum_length = 1;
            model->illegal_symbols = false;
            model->clear_default_text = false;
            model->cursor_pos = 0;
            model->cursor_select = false;
            model->text_buffer = NULL;
            model->text_buffer_size = 0;
            model->callback = NULL;
            model->callback_context = NULL;
            model->accent_return_row = 0;
            model->accent_return_column = 0;
            model->validator_callback = NULL;
            model->validator_callback_context = NULL;
            furi_string_reset(model->validator_text);
            model->validator_message_visible = false;
            model->is_password = false;
            model->for_open = false;
        },
        true);
}

View* text_input_get_view(TextInput* text_input) {
    furi_check(text_input);
    return text_input->view;
}

void text_input_set_result_callback(
    TextInput* text_input,
    TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            model->cursor_select = false;
            text_input_set_default_selection(model);
            model->accent_return_row = 0;
            model->accent_return_column = 0;
            if(text_buffer && text_buffer[0] != '\0') {
                model->cursor_pos = strlen(text_buffer);
            } else {
                model->cursor_pos = 0;
            }
        },
        true);
}

void text_input_set_minimum_length(TextInput* text_input, size_t minimum_length) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { model->minimum_length = minimum_length; },
        true);
}

void text_input_show_illegal_symbols(TextInput* text_input, bool show) {
    furi_check(text_input);
    with_view_model(
        text_input->view, TextInputModel * model, { model->illegal_symbols = show; }, true);
}

void text_input_set_is_password(TextInput* text_input, bool is_password) {
    furi_check(text_input);
    with_view_model(
        text_input->view, TextInputModel * model, { model->is_password = is_password; }, true);
}

void text_input_set_for_open(TextInput* text_input, bool for_open) {
    furi_check(text_input);
    with_view_model(
        text_input->view, TextInputModel * model, { model->for_open = for_open; }, true);
}

void text_input_set_validator(
    TextInput* text_input,
    TextInputValidatorCallback callback,
    void* callback_context) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->validator_callback = callback;
            model->validator_callback_context = callback_context;
        },
        true);
}

TextInputValidatorCallback text_input_get_validator_callback(TextInput* text_input) {
    furi_check(text_input);
    TextInputValidatorCallback validator_callback = NULL;
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { validator_callback = model->validator_callback; },
        false);
    return validator_callback;
}

void* text_input_get_validator_callback_context(TextInput* text_input) {
    furi_check(text_input);
    void* validator_callback_context = NULL;
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { validator_callback_context = model->validator_callback_context; },
        false);
    return validator_callback_context;
}

void text_input_set_header_text(TextInput* text_input, const char* text) {
    furi_check(text_input);
    with_view_model(text_input->view, TextInputModel * model, { model->header = text; }, true);
}
