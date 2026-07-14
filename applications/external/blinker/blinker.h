#pragma once

#include <furi.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/number_input.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

// View types
typedef enum {
    Main,
    Exec,
    NumberPicker,
} Views;

// NumberPicker Input modes
typedef enum {
    Min,
    Max,
    Dur,
} Modes;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Views current_view;

    FuriTimer* timer;
    DialogEx* dialog;
    NumberInput* number_input;
    Widget* widget;

    Modes mode;
    uint32_t duration;
    uint32_t min_interval;
    uint32_t max_interval;
    uint32_t start_time;
    uint32_t last_check;
    bool time_out;
} BlinkerApp;

static void main_view(BlinkerApp* app);
static void number_picker_view(
    BlinkerApp* app,
    const char* header,
    uint32_t current,
    uint32_t min,
    uint32_t max);
static void exec_view(BlinkerApp* app);

static void main_callback(DialogExResult result, void* context);
static void number_picker_callback(void* context, int32_t value);
static void timer_callback(void* context);
static bool back_button_callback(void* context);

int32_t blinker_main(void* p);
