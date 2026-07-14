#include "time_input.h"

#include <furi.h>
#include <gui/elements.h>
#include <infrared.h>
#include "../pause_timer.h"

#define MAX_TIME_S 9999

typedef enum {
    PT_INPUT_INVALID = -1,
    PT_INPUT_0 = 0,
    PT_INPUT_1 = 1,
    PT_INPUT_2 = 2,
    PT_INPUT_3 = 3,
    PT_INPUT_4 = 4,
    PT_INPUT_5 = 5,
    PT_INPUT_6 = 6,
    PT_INPUT_7 = 7,
    PT_INPUT_8 = 8,
    PT_INPUT_9 = 9,
    PT_INPUT_CLEAR = 10,
    PT_INPUT_DEL = 11,
    PT_INPUT_START = 12,
    PT_INPUT_LEARN = 13
} PTInputOption;

struct PTTimeInput {
    View* view;
    PauseTimerApp* pause_timer_app;
    TimeInputStartCallback start_callback;
    void* start_context;
    TimeInputLearnCallback learn_callback;
    void* learn_context;
};

typedef struct {
    uint8_t last_x;
    uint8_t last_y;
    uint8_t x;
    uint8_t y;
    uint8_t last_key_code;
    uint16_t modifier_code;
    bool ok_pressed;
    char key_string[10];
    uint16_t timer_val;
} TimeInputModel;

typedef struct {
    uint8_t width;
    char* key;
    uint8_t height;
    PTInputOption value;
} TimeInputKey;

typedef struct {
    int8_t x;
    int8_t y;
} Point;

#define MARGIN_TOP   33
#define MARGIN_LEFT  1
#define KEY_WIDTH    20
#define KEY_HEIGHT   15
#define KEY_PADDING  1
#define ROW_COUNT    6
#define COLUMN_COUNT 3

const TimeInputKey time_input_keyset[ROW_COUNT][COLUMN_COUNT] = {
    {
        {.width = 1, .height = 1, .key = "7", .value = PT_INPUT_7},
        {.width = 1, .height = 1, .key = "8", .value = PT_INPUT_8},
        {.width = 1, .height = 1, .key = "9", .value = PT_INPUT_9},
    },
    {
        {.width = 1, .height = 1, .key = "4", .value = PT_INPUT_4},
        {.width = 1, .height = 1, .key = "5", .value = PT_INPUT_5},
        {.width = 1, .height = 1, .key = "6", .value = PT_INPUT_6},
    },
    {
        {.width = 1, .height = 1, .key = "1", .value = PT_INPUT_1},
        {.width = 1, .height = 1, .key = "2", .value = PT_INPUT_2},
        {.width = 1, .height = 1, .key = "3", .value = PT_INPUT_3},
    },
    {
        {.width = 1, .height = 1, .key = "CLR", .value = PT_INPUT_CLEAR},
        {.width = 1, .height = 1, .key = "0", .value = PT_INPUT_0},
        {.width = 1, .height = 1, .key = "DEL", .value = PT_INPUT_DEL},
    },
    {
        {.width = 3, .height = 1, .key = "START", .value = PT_INPUT_START},
        {.width = 0, .height = 1, .key = "START", .value = PT_INPUT_START},
        {.width = 0, .height = 1, .key = "START", .value = PT_INPUT_START},
    },
    {
        {.width = 3, .height = 1, .key = "LEARN", .value = PT_INPUT_LEARN},
        {.width = 0, .height = 1, .key = "LEARN", .value = PT_INPUT_LEARN},
        {.width = 0, .height = 1, .key = "LEARN", .value = PT_INPUT_LEARN},
    },
};

static void time_input_draw_key(
    Canvas* canvas,
    TimeInputModel* model,
    uint8_t x,
    uint8_t y,
    TimeInputKey key,
    bool selected) {
    if(!key.width || !key.height) return;

    canvas_set_color(canvas, ColorBlack);
    uint8_t keyWidth = KEY_WIDTH * key.width + KEY_PADDING * (key.width - 1);
    uint8_t keyHeight = KEY_HEIGHT * key.height + KEY_PADDING * (key.height - 1);
    if(selected) {
        elements_slightly_rounded_box(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING),
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING),
            keyWidth,
            keyHeight);
        canvas_set_color(canvas, ColorWhite);
    } else {
        elements_slightly_rounded_frame(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING),
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING),
            keyWidth,
            keyHeight);
    }

    strncpy(model->key_string, key.key, sizeof(model->key_string) - 1);
    model->key_string[sizeof(model->key_string) - 1] = '\0';
    canvas_draw_str_aligned(
        canvas,
        MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING) + keyWidth / 2 + 1,
        MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING) + keyHeight / 2 + 1,
        AlignCenter,
        AlignCenter,
        model->key_string);
}

static FuriString* get_timer_string(uint16_t timer_val) {
    FuriString* temp = furi_string_alloc();
    if(timer_val > MAX_TIME_S) {
        timer_val %= MAX_TIME_S + 1;
    }

    // Grab the two digit minute
    uint8_t min = timer_val / 100;
    uint8_t sec = timer_val % 100;
    furi_string_cat_printf(temp, "%02d:%02d", min, sec);

    return temp;
}

static void make_input(TimeInputModel* model, PTInputOption input) {
    uint32_t temp_val = model->timer_val;

    if(input == PT_INPUT_INVALID) {
        // Do nothing
    } else if(input == PT_INPUT_DEL) {
        temp_val /= 10;
    } else if(input == PT_INPUT_CLEAR) {
        temp_val = 0;
    } else if(input == PT_INPUT_START) {
        // START button handled separately
    } else if(input == PT_INPUT_LEARN) {
        // LEARN button handled separately
    } else {
        temp_val *= 10;
        temp_val += input;
    }

    // Wrap if we exceed the max time
    if(temp_val > MAX_TIME_S) {
        temp_val %= MAX_TIME_S + 1;
    }

    // Double check we are in range
    // I don't remember why I wrote this but might be useful
    model->timer_val = temp_val & 0xFFFF;
}

static void time_input_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    TimeInputModel* model = context;

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 0, 1, AlignLeft, AlignTop, "Pause Timer");

    canvas_set_font(canvas, FontBigNumbers);
    FuriString* timer_str = get_timer_string(model->timer_val);
    elements_multiline_text_aligned(
        canvas, 3, 14, AlignLeft, AlignTop, furi_string_get_cstr(timer_str));
    furi_string_free(timer_str);

    canvas_set_font(canvas, FontKeyboard);
    uint8_t initY = 0;

    for(uint8_t y = initY; y < ROW_COUNT; y++) {
        const TimeInputKey* numpadKeyRow = time_input_keyset[y];
        uint8_t x = 0;
        for(uint8_t i = 0; i < COLUMN_COUNT; i++) {
            TimeInputKey key = numpadKeyRow[i];
            bool keySelected = (x <= model->x && model->x < (x + key.width)) && y == model->y;
            time_input_draw_key(
                canvas, model, x, y - initY, key, (!model->ok_pressed && keySelected));
            x += key.width;
        }
    }
}

static PTInputOption time_input_get_selected_key(TimeInputModel* model) {
    TimeInputKey key = time_input_keyset[model->y][model->x];
    return key.value;
}

static void time_input_get_select_key(TimeInputModel* model, Point delta) {
    do {
        const int delta_sum = model->y + delta.y;
        model->y = delta_sum < 0 ? ROW_COUNT - 1 : delta_sum % ROW_COUNT;
    } while(delta.y != 0 && time_input_keyset[model->y][model->x].value == -1);

    do {
        const int delta_sum = model->x + delta.x;
        model->x = delta_sum < 0 ? COLUMN_COUNT - 1 : delta_sum % COLUMN_COUNT;
    } while(delta.x != 0 && time_input_keyset[model->y][model->x].width == 0);
}

static void time_input_process(PTTimeInput* time_input, InputEvent* event) {
    with_view_model(
        time_input->view,
        TimeInputModel * model,
        {
            if(event->key == InputKeyOk) {
                if(event->type == InputTypePress) {
                    model->ok_pressed = true;
                } else if(event->type == InputTypeLong || event->type == InputTypeShort) {
                    model->last_key_code = time_input_get_selected_key(model);

                    if(model->last_key_code == PT_INPUT_START) {
                        if(time_input->start_callback) {
                            time_input->start_callback(
                                time_input->start_context, model->timer_val);
                        }
                    } else if(model->last_key_code == PT_INPUT_LEARN) {
                        if(time_input->learn_callback) {
                            time_input->learn_callback(time_input->learn_context);
                        }
                    } else {
                        make_input(model, model->last_key_code);
                    }
                } else if(event->type == InputTypeRelease) {
                    model->ok_pressed = false;
                }
            } else if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                if(event->key == InputKeyUp) {
                    time_input_get_select_key(model, (Point){.x = 0, .y = -1});
                } else if(event->key == InputKeyDown) {
                    time_input_get_select_key(model, (Point){.x = 0, .y = 1});
                } else if(event->key == InputKeyLeft) {
                    if(model->last_x == 2 && model->last_y == 2 && model->y == 1 &&
                       model->x == 3) {
                        model->x = model->last_x;
                        model->y = model->last_y;
                    } else if(
                        model->last_x == 2 && model->last_y == 4 && model->y == 3 &&
                        model->x == 3) {
                        model->x = model->last_x;
                        model->y = model->last_y;
                    } else
                        time_input_get_select_key(model, (Point){.x = -1, .y = 0});
                    model->last_x = 0;
                    model->last_y = 0;
                } else if(event->key == InputKeyRight) {
                    if(model->x == 2 && model->y == 2) {
                        model->last_x = model->x;
                        model->last_y = model->y;
                        time_input_get_select_key(model, (Point){.x = 1, .y = -1});
                    } else if(model->x == 2 && model->y == 4) {
                        model->last_x = model->x;
                        model->last_y = model->y;
                        time_input_get_select_key(model, (Point){.x = 1, .y = -1});
                    } else {
                        time_input_get_select_key(model, (Point){.x = 1, .y = 0});
                    }
                }
            }
        },
        true);
}

static bool time_input_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    PTTimeInput* time_input = context;
    bool consumed = false;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        // Used to release keys
    } else {
        time_input_process(time_input, event);
        consumed = true;
    }

    return consumed;
}

PTTimeInput* time_input_alloc(PauseTimerApp* pt_app) {
    PTTimeInput* time_input = malloc(sizeof(PTTimeInput));
    time_input->view = view_alloc();
    time_input->pause_timer_app = pt_app;
    time_input->start_callback = NULL;
    time_input->start_context = NULL;
    time_input->learn_callback = NULL;
    time_input->learn_context = NULL;

    view_set_context(time_input->view, time_input);
    view_allocate_model(time_input->view, ViewModelTypeLocking, sizeof(TimeInputModel));
    view_set_orientation(time_input->view, ViewOrientationVertical);
    view_set_draw_callback(time_input->view, time_input_draw_callback);
    view_set_input_callback(time_input->view, time_input_input_callback);

    with_view_model(
        time_input->view,
        TimeInputModel * model,
        {
            model->y = 0;
            model->x = 0;
            model->last_x = 0;
            model->last_y = 0;
            model->last_key_code = 0;
            model->modifier_code = 0;
            model->ok_pressed = false;
            model->timer_val = 0;
            memset(model->key_string, 0, sizeof(model->key_string));
        },
        true);

    return time_input;
}

void time_input_free(PTTimeInput* time_input) {
    furi_assert(time_input);
    view_free(time_input->view);
    free(time_input);
}

View* time_input_get_view(PTTimeInput* time_input) {
    furi_assert(time_input);
    return time_input->view;
}

void time_input_set_start_callback(
    PTTimeInput* time_input,
    TimeInputStartCallback callback,
    void* context) {
    furi_assert(time_input);
    time_input->start_callback = callback;
    time_input->start_context = context;
}

void time_input_set_learn_callback(
    PTTimeInput* time_input,
    TimeInputLearnCallback callback,
    void* context) {
    furi_assert(time_input);
    time_input->learn_callback = callback;
    time_input->learn_context = context;
}
