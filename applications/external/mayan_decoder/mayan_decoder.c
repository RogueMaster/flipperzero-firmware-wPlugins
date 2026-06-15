#include <furi.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAYAN_DECODER_MIN_VALUE 0
#define MAYAN_DECODER_MAX_VALUE 9999
#define MAYAN_DECODER_QUEUE_SIZE 8
#define MAYAN_DECODER_EDIT_DIGITS 4

typedef enum {
    MayanDecoderScreenSplash,
    MayanDecoderScreenMain,
    MayanDecoderScreenEdit,
} MayanDecoderScreen;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    MayanDecoderScreen screen;
    int32_t value;
    int32_t edit_value;
    uint8_t edit_digit;
} MayanDecoderApp;

static int32_t mayan_decoder_clamp_value(int32_t value) {
    if(value < MAYAN_DECODER_MIN_VALUE) {
        return MAYAN_DECODER_MIN_VALUE;
    }

    if(value > MAYAN_DECODER_MAX_VALUE) {
        return MAYAN_DECODER_MAX_VALUE;
    }

    return value;
}

static int32_t mayan_decoder_digit_place(uint8_t digit_index) {
    static const int32_t places[MAYAN_DECODER_EDIT_DIGITS] = {1000, 100, 10, 1};
    furi_assert(digit_index < COUNT_OF(places));
    return places[digit_index];
}

static uint8_t mayan_decoder_decimal_digit(int32_t value, uint8_t digit_index) {
    const int32_t place = mayan_decoder_digit_place(digit_index);
    return (value / place) % 10;
}

static void mayan_decoder_draw_shell(Canvas* canvas, int32_t center_x, int32_t top_y) {
    canvas_draw_circle(canvas, center_x, top_y + 3, 3);
    canvas_draw_line(canvas, center_x - 3, top_y + 3, center_x + 3, top_y + 3);
    canvas_draw_line(canvas, center_x - 2, top_y + 5, center_x + 2, top_y + 5);
    canvas_draw_dot(canvas, center_x - 2, top_y + 2);
    canvas_draw_dot(canvas, center_x, top_y + 1);
    canvas_draw_dot(canvas, center_x + 2, top_y + 2);
}

static void mayan_decoder_draw_digit(Canvas* canvas, int32_t digit, int32_t center_x, int32_t top_y) {
    furi_assert(digit >= 0);
    furi_assert(digit < 20);

    if(digit == 0) {
        mayan_decoder_draw_shell(canvas, center_x, top_y);
        return;
    }

    const int32_t bars = digit / 5;
    const int32_t dots = digit % 5;
    const int32_t bar_width = 22;
    const int32_t bar_y = top_y + 6;

    for(int32_t i = 0; i < bars; i++) {
        const int32_t y = bar_y - (bars - 1 - i);
        canvas_draw_line(canvas, center_x - (bar_width / 2), y, center_x + (bar_width / 2), y);
    }

    if(dots > 0) {
        const int32_t dot_y = (bars > 0) ? top_y + 1 : top_y + 4;
        const int32_t dot_spacing = 5;
        const int32_t start_x = center_x - ((dots - 1) * dot_spacing) / 2;
        for(int32_t i = 0; i < dots; i++) {
            canvas_draw_disc(canvas, start_x + (i * dot_spacing), dot_y, 1);
        }
    }
}

static uint8_t mayan_decoder_to_base20(int32_t value, int32_t* digits, size_t max_digits) {
    furi_assert(digits);
    furi_assert(max_digits > 0);

    if(value == 0) {
        digits[0] = 0;
        return 1;
    }

    uint8_t count = 0;
    while(value > 0 && count < max_digits) {
        digits[count++] = value % 20;
        value /= 20;
    }

    return count;
}

static void mayan_decoder_draw_pyramid(Canvas* canvas, int32_t center_x, int32_t base_y) {
    const uint8_t widths[] = {12, 24, 36, 50, 64};

    for(uint8_t i = 0; i < COUNT_OF(widths); i++) {
        const int32_t width = widths[i];
        const int32_t y = base_y - ((COUNT_OF(widths) - i) * 5);
        canvas_draw_frame(canvas, center_x - (width / 2), y, width, 5);
    }

    canvas_draw_frame(canvas, center_x - 8, base_y - 31, 16, 7);
    canvas_draw_line(canvas, center_x - 3, base_y - 24, center_x - 3, base_y - 20);
    canvas_draw_line(canvas, center_x + 3, base_y - 24, center_x + 3, base_y - 20);
    canvas_draw_line(canvas, center_x - 31, base_y, center_x + 31, base_y);
}

static void mayan_decoder_draw_mayan_number(Canvas* canvas, int32_t value) {
    int32_t digits[4] = {0};
    const uint8_t digit_count = mayan_decoder_to_base20(value, digits, COUNT_OF(digits));
    const int32_t tier_height = 7;
    const int32_t tier_gap = 1;
    const int32_t total_height = (digit_count * tier_height) + ((digit_count - 1) * tier_gap);
    int32_t y = 27 + ((31 - total_height) / 2);

    for(int32_t i = digit_count - 1; i >= 0; i--) {
        mayan_decoder_draw_digit(canvas, digits[i], 64, y);

        if(i > 0) {
            canvas_draw_line(canvas, 46, y + tier_height, 82, y + tier_height);
        }

        y += tier_height + tier_gap;
    }
}

static void mayan_decoder_draw_splash(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "Mayan Decoder");

    canvas_draw_circle(canvas, 17, 19, 5);
    canvas_draw_line(canvas, 17, 10, 17, 13);
    canvas_draw_line(canvas, 17, 25, 17, 29);
    canvas_draw_line(canvas, 8, 19, 11, 19);
    canvas_draw_line(canvas, 23, 19, 27, 19);
    canvas_draw_line(canvas, 11, 13, 13, 15);
    canvas_draw_line(canvas, 23, 13, 21, 15);

    mayan_decoder_draw_pyramid(canvas, 72, 49);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 59, AlignCenter, AlignBottom, "OK Start   Back Exit");
}

static void mayan_decoder_draw_main(Canvas* canvas, const MayanDecoderApp* app) {
    char value_text[24];
    snprintf(value_text, sizeof(value_text), "%ld", (long)app->value);

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_line(canvas, 0, 13, 127, 13);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 10, "Mayan Decoder");
    canvas_draw_str(canvas, 89, 10, "B Exit");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, value_text);

    canvas_draw_line(canvas, 37, 26, 91, 26);
    mayan_decoder_draw_mayan_number(canvas, app->value);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Arrows +/-   OK Input");
}

static void mayan_decoder_draw_edit_digit(
    Canvas* canvas,
    uint8_t digit,
    uint8_t digit_index,
    bool selected) {
    const int32_t x = 28 + (digit_index * 18);
    const int32_t y = 28;
    char digit_text[2] = {(char)('0' + digit), '\0'};

    if(selected) {
        canvas_draw_box(canvas, x, y, 15, 18);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 7, y + 14, AlignCenter, AlignBottom, digit_text);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_frame(canvas, x, y, 15, 18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 7, y + 14, AlignCenter, AlignBottom, digit_text);
    }
}

static void mayan_decoder_draw_edit(Canvas* canvas, const MayanDecoderApp* app) {
    char range_text[24];
    snprintf(range_text, sizeof(range_text), "0-%ld", (long)MAYAN_DECODER_MAX_VALUE);

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "Enter Number");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignBottom, range_text);

    for(uint8_t i = 0; i < MAYAN_DECODER_EDIT_DIGITS; i++) {
        const uint8_t digit = mayan_decoder_decimal_digit(app->edit_value, i);
        mayan_decoder_draw_edit_digit(canvas, digit, i, i == app->edit_digit);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignBottom, "L/R Digit   U/D +/-");
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK Save   Back Cancel");
}

static void mayan_decoder_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    MayanDecoderApp* app = context;

    switch(app->screen) {
    case MayanDecoderScreenSplash:
        mayan_decoder_draw_splash(canvas);
        break;
    case MayanDecoderScreenMain:
        mayan_decoder_draw_main(canvas, app);
        break;
    case MayanDecoderScreenEdit:
        mayan_decoder_draw_edit(canvas, app);
        break;
    default:
        furi_crash("Invalid screen");
    }
}

static void mayan_decoder_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    MayanDecoderApp* app = context;
    furi_message_queue_put(app->input_queue, event, 0);
}

static void mayan_decoder_change_value(MayanDecoderApp* app, int32_t delta) {
    furi_assert(app);

    const int32_t next_value = mayan_decoder_clamp_value(app->value + delta);
    if(next_value != app->value) {
        app->value = next_value;
        view_port_update(app->view_port);
    }
}

static void mayan_decoder_change_edit_digit(MayanDecoderApp* app, int32_t delta) {
    furi_assert(app);

    const uint8_t current_digit = mayan_decoder_decimal_digit(app->edit_value, app->edit_digit);
    const uint8_t next_digit = (current_digit + 10 + delta) % 10;
    const int32_t place = mayan_decoder_digit_place(app->edit_digit);
    int32_t next_value = app->edit_value + ((int32_t)next_digit - current_digit) * place;

    if(next_value > MAYAN_DECODER_MAX_VALUE) {
        next_value = MAYAN_DECODER_MAX_VALUE;
    }

    app->edit_value = mayan_decoder_clamp_value(next_value);
    view_port_update(app->view_port);
}

static void mayan_decoder_handle_splash(MayanDecoderApp* app, const InputEvent* event, bool* running) {
    if(event->key == InputKeyBack) {
        *running = false;
        return;
    }

    app->screen = MayanDecoderScreenMain;
    view_port_update(app->view_port);
}

static void mayan_decoder_handle_main(MayanDecoderApp* app, const InputEvent* event, bool* running) {
    switch(event->key) {
    case InputKeyUp:
    case InputKeyRight:
        mayan_decoder_change_value(app, 1);
        break;
    case InputKeyDown:
    case InputKeyLeft:
        mayan_decoder_change_value(app, -1);
        break;
    case InputKeyOk:
        app->edit_value = app->value;
        app->edit_digit = 0;
        app->screen = MayanDecoderScreenEdit;
        view_port_update(app->view_port);
        break;
    case InputKeyBack:
        *running = false;
        break;
    default:
        break;
    }
}

static void mayan_decoder_handle_edit(MayanDecoderApp* app, const InputEvent* event) {
    switch(event->key) {
    case InputKeyLeft:
        if(app->edit_digit > 0) {
            app->edit_digit--;
        }
        view_port_update(app->view_port);
        break;
    case InputKeyRight:
        if(app->edit_digit < MAYAN_DECODER_EDIT_DIGITS - 1) {
            app->edit_digit++;
        }
        view_port_update(app->view_port);
        break;
    case InputKeyUp:
        mayan_decoder_change_edit_digit(app, 1);
        break;
    case InputKeyDown:
        mayan_decoder_change_edit_digit(app, -1);
        break;
    case InputKeyOk:
        app->value = mayan_decoder_clamp_value(app->edit_value);
        app->screen = MayanDecoderScreenMain;
        view_port_update(app->view_port);
        break;
    case InputKeyBack:
        app->screen = MayanDecoderScreenMain;
        view_port_update(app->view_port);
        break;
    default:
        break;
    }
}

int32_t mayan_decoder_app(void* p) {
    UNUSED(p);

    MayanDecoderApp* app = malloc(sizeof(MayanDecoderApp));
    furi_assert(app);
    app->input_queue = furi_message_queue_alloc(MAYAN_DECODER_QUEUE_SIZE, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->screen = MayanDecoderScreenSplash;
    app->value = 0;
    app->edit_value = 0;
    app->edit_digit = 0;

    view_port_draw_callback_set(app->view_port, mayan_decoder_draw_callback, app);
    view_port_input_callback_set(app->view_port, mayan_decoder_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    bool running = true;
    InputEvent event;
    while(running) {
        const FuriStatus status = furi_message_queue_get(app->input_queue, &event, FuriWaitForever);
        if(status != FuriStatusOk) {
            continue;
        }

        if(event.type != InputTypeShort && event.type != InputTypeRepeat) {
            continue;
        }

        switch(app->screen) {
        case MayanDecoderScreenSplash:
            mayan_decoder_handle_splash(app, &event, &running);
            break;
        case MayanDecoderScreenMain:
            mayan_decoder_handle_main(app, &event, &running);
            break;
        case MayanDecoderScreenEdit:
            mayan_decoder_handle_edit(app, &event);
            break;
        default:
            furi_crash("Invalid screen");
        }
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
