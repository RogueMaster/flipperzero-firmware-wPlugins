

#include "flipp_pomodoro_config_view.h"
#include "../modules/flipp_pomodoro_settings.h"
#include <furi.h>

#include <gui/icon_i.h>
#include <gui/view.h>
#include <gui/canvas.h>

#include <gui/elements.h>



const uint8_t _I_back_10px_0[] = {0x00,0x00,0x00,0x10,0x00,0x38,0x00,0x7c,0x00,0xfe,0x00,0x38,0x00,0x38,0x00,0xf8,0x01,0xf8,0x01,0x00,0x00,};
const uint8_t* const _I_back_10px[] = {_I_back_10px_0};
const Icon I_back_10px = {.width=10,.height=10,.frame_count=1,.frame_rate=0,.frames=_I_back_10px};

typedef struct {
    uint8_t selected;     // index of the selected row: 0=focus, 1=short, 2=long, 3=buzz mode
    uint8_t durations[3]; // minutes for the first 3 rows (displayed as number + " min")
    uint8_t buzz_mode;    // notification mode
} FlippPomodoroConfigViewModel;

struct FlippPomodoroConfigView {
    View* view;
    FlippPomodoroConfigViewActionCb on_save_cb;
    void* cb_ctx;
};

static const char* buzz_mode_to_str(uint8_t m) {
    switch(m) {
        case FlippPomodoroBuzzSlide: return "Slide";
        case FlippPomodoroBuzzOnce: return "Once";
        case FlippPomodoroBuzzAnnoying: return "Naggy";
        case FlippPomodoroBuzzFlash: return "Flash";
        case FlippPomodoroBuzzVibrate: return "Vibrate";
        case FlippPomodoroBuzzSoftBeep: return "Beep soft";
        case FlippPomodoroBuzzLoudBeep: return "Beep loud";
        default: return "?";
    }
}

static uint8_t buzz_mode_sanitise(uint8_t mode) {
    if(mode >= FlippPomodoroBuzzModeCount) {
        return FlippPomodoroBuzzOnce;
    }
    return mode;
}

static uint8_t buzz_mode_next(uint8_t mode) {
    return (mode + 1) % FlippPomodoroBuzzModeCount;
}

static uint8_t buzz_mode_prev(uint8_t mode) {
    return (mode + FlippPomodoroBuzzModeCount - 1) % FlippPomodoroBuzzModeCount;
}


void elements_button_back(Canvas* canvas, const char* str) {
    furi_check(canvas);

    const size_t button_height = 12;
    const size_t vertical_offset = 3;
    const size_t horizontal_offset = 3;
    const size_t string_width = canvas_string_width(canvas, str);
    const Icon* icon = &I_back_10px;
    const int32_t icon_h_offset = 3;
    const int32_t icon_width_with_offset = icon->width + icon_h_offset;
    const int32_t icon_v_offset = icon->height + 1;
    const size_t button_width = string_width + horizontal_offset * 2 + icon_width_with_offset;

    const int32_t x = canvas_width(canvas);
    const int32_t y = canvas_height(canvas);

    canvas_draw_box(canvas, x - button_width, y - button_height, button_width, button_height);
    canvas_draw_line(canvas, x - button_width - 1, y, x - button_width - 1, y - button_height + 0);
    canvas_draw_line(canvas, x - button_width - 2, y, x - button_width - 2, y - button_height + 1);
    canvas_draw_line(canvas, x - button_width - 3, y, x - button_width - 3, y - button_height + 2);

    canvas_invert_color(canvas);
    canvas_draw_str(canvas, x - button_width + horizontal_offset, y - vertical_offset, str);
    canvas_draw_icon(
        canvas, x - horizontal_offset - icon->width, y - icon_v_offset, icon);
    canvas_invert_color(canvas);
}

static void config_draw_callback(Canvas* canvas, void* ctx) {
    FlippPomodoroConfigViewModel* model = ctx;
    canvas_clear(canvas); // clear the screen

    const char* labels[4] = {"Focus:", "Short Break:", "Long Break:", "Buzz Mode:"}; // labels on the left (x_label)

    // Vertical layout of text:
    const uint8_t y_base = 10;  // base Y for the first row
    const uint8_t row_h = 12;   // vertical spacing between rows

    for(uint8_t i = 0; i < 4; i++) { // 4 UI rows
        bool sel = (i == model->selected); // currently selected row

        // Horizontal coordinates of columns:
        uint8_t x_label = 5 + 2; // X of label (left of selection box starting at x=5)
        uint8_t y = y_base + row_h * i; // Y of the current row
        uint8_t x_left = 70;   // X for left arrow "<"
        uint8_t x_val = 78;    // X for value (minutes or buzz mode)
        uint8_t x_right = 118; // X for right arrow ">"

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 5, y - 9, 118, 12); // selection box: (x=5, y=y-9, w=118, h=12)
            canvas_set_color(canvas, ColorWhite);       // inverted text inside selection
        } else {
            canvas_set_color(canvas, ColorBlack);       // normal text color
        }

        canvas_set_font(canvas, FontPrimary);           // font: primary (FontPrimary)
        canvas_draw_str(canvas, x_label, y, labels[i]); // draw label at (x_label, y)
        canvas_draw_str(canvas, x_left, y, "<");        // left arrow at (x_left, y)

        if(i < 3) {
            char val_buf[8];
            snprintf(val_buf, sizeof(val_buf), "%2u min", model->durations[i]); // format value as minutes
            canvas_draw_str(canvas, x_val, y, val_buf);     // draw value at (x_val, y)
        } else {
            canvas_draw_str(canvas, x_val, y, buzz_mode_to_str(model->buzz_mode)); // draw buzz mode at (x_val, y)
        }

        canvas_draw_str(canvas, x_right, y, ">");       // right arrow at (x_right, y)
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
    elements_button_center(canvas, "Save"); // soft-button centered on the bottom bar
    elements_button_back(canvas, "Back");  // soft-button on the right of the bottom bar
}

static bool config_input_callback(InputEvent* event, void* ctx) {
    FlippPomodoroConfigView* self = ctx;
    bool handled = false;

    if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
        with_view_model(self->view, FlippPomodoroConfigViewModel * model, {
            switch(event->key) {
                case InputKeyUp:
                    if(model->selected > 0) model->selected--;  // move up (0..3)
                    handled = true;
                    break;
                case InputKeyDown:
                    if(model->selected < 3) model->selected++;  // move down (0..3)
                    handled = true;
                    break;
                case InputKeyRight:
                    if(model->selected < 3) {
                        if(model->durations[model->selected] < 99) model->durations[model->selected]++; // increment minutes
                    } else {
                        model->buzz_mode = buzz_mode_next(model->buzz_mode); // next buzz mode
                    }
                    handled = true;
                    break;
                case InputKeyLeft:
                    if(model->selected < 3) {
                        if(model->durations[model->selected] > 1) model->durations[model->selected]--; // decrement minutes
                    } else {
                        model->buzz_mode = buzz_mode_prev(model->buzz_mode); // previous mode (circular)
                    }
                    handled = true;
                    break;
                case InputKeyOk:
                    if(self->on_save_cb) self->on_save_cb(self->cb_ctx); // confirmation (Save)
                    handled = true;
                    break;
                default:
                    break;
            }
        }, true);
    }

    return handled;
}

FlippPomodoroConfigView* flipp_pomodoro_view_config_alloc() {
    FlippPomodoroConfigView* config = malloc(sizeof(FlippPomodoroConfigView));
    config->view = view_alloc();
    config->on_save_cb = NULL;
    config->cb_ctx = NULL;

    view_allocate_model(config->view, ViewModelTypeLockFree, sizeof(FlippPomodoroConfigViewModel));
    view_set_context(config->view, config);
    view_set_draw_callback(config->view, config_draw_callback);
    view_set_input_callback(config->view, config_input_callback);

    with_view_model(config->view, FlippPomodoroConfigViewModel * model, {
        model->selected = 0;     // first row (Focus) is selected by default
        model->durations[0] = 0; // default minutes
        model->durations[1] = 0;
        model->durations[2] = 0;
        model->buzz_mode = FlippPomodoroBuzzOnce; // default buzz mode
    }, false);

    return config;
}

View* flipp_pomodoro_view_config_get_view(FlippPomodoroConfigView* config) {
    return config->view;
}

void flipp_pomodoro_view_config_free(FlippPomodoroConfigView* config) {
    view_free(config->view);
    free(config);
}

void flipp_pomodoro_view_config_set_on_save_cb(
    FlippPomodoroConfigView* view,
    FlippPomodoroConfigViewActionCb cb,
    void* ctx
) {
    view->on_save_cb = cb;
    view->cb_ctx = ctx;
}

void flipp_pomodoro_view_config_set_settings(
    FlippPomodoroConfigView* view,
    const FlippPomodoroSettings* in
) {
    furi_assert(in);
    with_view_model(view->view, FlippPomodoroConfigViewModel * model, {
        model->durations[0] = in->focus_minutes;        // load minutes into model
        model->durations[1] = in->short_break_minutes;
        model->durations[2] = in->long_break_minutes;
        model->buzz_mode = buzz_mode_sanitise(in->buzz_mode); // load buzz mode
    }, true);
}

void flipp_pomodoro_view_config_get_settings(
    FlippPomodoroConfigView* view,
    FlippPomodoroSettings* out
) {
    furi_assert(out);
    with_view_model(view->view, FlippPomodoroConfigViewModel * model, {
        out->focus_minutes = model->durations[0];       // read minutes from model
        out->short_break_minutes = model->durations[1];
        out->long_break_minutes = model->durations[2];
        out->buzz_mode = model->buzz_mode;              // read buzz mode
    }, false);
}
