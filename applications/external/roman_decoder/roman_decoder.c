#include <input/input.h>
#include <gui/gui.h>
#include <furi.h>
#include <string.h>

#define TAG              "Roman Decoder"
#define DEBOUNCE_TIME_MS 300
#define MAX_ROMAN_LENGTH 15
#define MAX_REPEAT       3
#define MAX_VALUE        3999

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    char roman_input[32];
    char displayString[64];
    int current_char_index;
    uint32_t last_input_time;
    int current_value;
} RomanDecoder;

const char* roman_chars[] = {"M", "D", "C", "L", "X", "V", "I"};
const int roman_chars_count = sizeof(roman_chars) / sizeof(roman_chars[0]);

void draw_callback(Canvas* canvas, void* context) {
    RomanDecoder* app = (RomanDecoder*)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Roman Decoder");
    canvas_set_bitmap_mode(canvas, true);
    canvas_set_font(canvas, FontSecondary);

    int symbol_positions[] = {24, 38, 52, 64, 76, 90, 104};

    for(int i = 0; i < roman_chars_count; i++) {
        canvas_draw_str(canvas, symbol_positions[i], 30, roman_chars[i]);
        if(i == app->current_char_index) {
            canvas_draw_str(canvas, symbol_positions[i], 30 + 4, "_");
        }
    }

    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, app->roman_input);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, app->displayString);
}

void input_callback(InputEvent* event, void* context) {
    RomanDecoder* app = context;
    furi_message_queue_put(app->input_queue, event, 0);
}

int get_roman_value(char c) {
    switch(c) {
    case 'I':
        return 1;
    case 'V':
        return 5;
    case 'X':
        return 10;
    case 'L':
        return 50;
    case 'C':
        return 100;
    case 'D':
        return 500;
    case 'M':
        return 1000;
    default:
        return 0;
    }
}

int is_valid_roman(const char* s) {
    int len = (int)strlen(s);
    if(len == 0) return 1;
    if(len > MAX_ROMAN_LENGTH) return 0;
    for(int i = 0; i < len; i++) {
        if(get_roman_value(s[i]) == 0) return 0;
    }

    int repeat_count = 1;
    for(int i = 1; i < len; i++) {
        if(s[i] == s[i - 1]) {
            repeat_count++;
            if(s[i] == 'D' || s[i] == 'L' || s[i] == 'V') {
                if(repeat_count > 1) return 0;
            } else if(s[i] == 'M' || s[i] == 'C' || s[i] == 'X' || s[i] == 'I') {
                if(repeat_count > MAX_REPEAT) return 0;
            }
        } else {
            repeat_count = 1;
        }
    }
    for(int i = 0; i < len - 1; i++) {
        int current = get_roman_value(s[i]);
        int next = get_roman_value(s[i + 1]);
        if(current < next) {
            if(!((s[i] == 'I' && (s[i + 1] == 'V' || s[i + 1] == 'X')) ||
                 (s[i] == 'X' && (s[i + 1] == 'L' || s[i + 1] == 'C')) ||
                 (s[i] == 'C' && (s[i + 1] == 'D' || s[i + 1] == 'M')))) {
                return 0;
            }
            if(i > 0 && get_roman_value(s[i - 1]) < next) {
                return 0;
            }
            if(next > current * 10) {
                return 0;
            }
        }
    }

    return 1;
}

int roman_to_integer(const char* s) {
    if(!is_valid_roman(s)) {
        return -1;
    }

    int total = 0;
    int prev_value = 0;
    int len = (int)strlen(s);
    for(int i = len - 1; i >= 0; i--) {
        int current = get_roman_value(s[i]);
        if(current < prev_value) {
            total -= current;
        } else {
            total += current;
        }
        prev_value = current;
    }
    if(total > MAX_VALUE) {
        return -2;
    }

    return total;
}

void handle_ok_key(RomanDecoder* app) {
    size_t current_len = strlen(app->roman_input);
    app->roman_input[current_len] = roman_chars[app->current_char_index][0];
    app->roman_input[current_len + 1] = '\0';
    int decimal = roman_to_integer(app->roman_input);
    if(decimal == -1) {
        app->roman_input[current_len] = '\0';
        snprintf(app->displayString, sizeof(app->displayString), "Invalid!");
        return;
    } else if(decimal == -2) {
        app->roman_input[current_len] = '\0';
        snprintf(app->displayString, sizeof(app->displayString), "Max: 3999");
        return;
    } else if(decimal > 0) {
        app->current_value = decimal;
        snprintf(app->displayString, sizeof(app->displayString), "= %d", decimal);
    } else {
        app->current_value = 0;
        snprintf(app->displayString, sizeof(app->displayString), "= 0");
    }
}

void handle_back_key(RomanDecoder* app) {
    size_t len = strlen(app->roman_input);
    if(len > 0) {
        app->roman_input[len - 1] = '\0';
        if(strlen(app->roman_input) == 0) {
            snprintf(app->displayString, sizeof(app->displayString), "Ready");
            app->current_value = 0;
        } else {
            int decimal = roman_to_integer(app->roman_input);
            if(decimal > 0) {
                app->current_value = decimal;
                snprintf(app->displayString, sizeof(app->displayString), "= %d", decimal);
            } else if(decimal == 0) {
                app->current_value = 0;
                snprintf(app->displayString, sizeof(app->displayString), "= 0");
            } else {
                snprintf(app->displayString, sizeof(app->displayString), "Invalid");
            }
        }
    }
}

void reset_app(RomanDecoder* app) {
    app->roman_input[0] = '\0';
    snprintf(app->displayString, sizeof(app->displayString), "Ready");
    app->current_char_index = 0;
    app->current_value = 0;
}

int32_t roman_decoder_main(void* p) {
    UNUSED(p);

    RomanDecoder app;
    reset_app(&app);
    app.last_input_time = 0;
    app.view_port = view_port_alloc();
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);
    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);
    InputEvent input;
    FURI_LOG_I(TAG, "Roman Decoder started");
    while(1) {
        furi_check(
            furi_message_queue_get(app.input_queue, &input, FuriWaitForever) == FuriStatusOk);

        uint32_t current_time = furi_get_tick();

        if(current_time - app.last_input_time < DEBOUNCE_TIME_MS) {
            view_port_update(app.view_port);
            continue;
        }

        switch(input.key) {
        case InputKeyRight:
            app.current_char_index = (app.current_char_index + 1) % roman_chars_count;
            app.last_input_time = current_time;
            break;

        case InputKeyLeft:
            app.current_char_index =
                (app.current_char_index - 1 + roman_chars_count) % roman_chars_count;
            app.last_input_time = current_time;
            break;

        case InputKeyOk:
            handle_ok_key(&app);
            app.last_input_time = current_time;
            break;

        case InputKeyBack:
            if(input.type == InputTypeLong) {
                FURI_LOG_I(TAG, "Long Back pressed - exiting");
                break;
            } else {
                handle_back_key(&app);
                app.last_input_time = current_time;
            }
            break;

        case InputKeyUp:
            reset_app(&app);
            app.last_input_time = current_time;
            break;
        default:
            break;
        }
        view_port_update(app.view_port);
        if(input.type == InputTypeLong && input.key == InputKeyBack) {
            break;
        }
    }

    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);

    return 0;
}
