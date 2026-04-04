#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_infrared.h>
#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <stdlib.h>
#include <stdio.h>

#define FLIPIRFREQ_DEFAULT_FREQUENCY 38000U
#define FLIPIRFREQ_DEFAULT_DUTY_CYCLE 33U
#define FLIPIRFREQ_DEFAULT_BURST_MS 250U
#define FLIPIRFREQ_DEFAULT_SIGNAL_MODE FlipIRFreqSignalModeCarrier
#define FLIPIRFREQ_TX_CHUNK_US 25000U
#define FLIPIRFREQ_UI_TICK_HZ 8U
#define FLIPIRFREQ_SETTINGS_PATH APP_DATA_PATH("flipirfreq.settings")
#define FLIPIRFREQ_SETTINGS_MAGIC 0x49
#define FLIPIRFREQ_SETTINGS_VERSION 1U
#define FLIPIRFREQ_PULSE_MIN_FREQUENCY_TENTHS 100U
#define FLIPIRFREQ_PULSE_MAX_FREQUENCY_TENTHS 5000U
#define FLIPIRFREQ_MIN_DUTY_CYCLE 1U
#define FLIPIRFREQ_MAX_DUTY_CYCLE 99U
#define FLIPIRFREQ_MIN_BURST_MS 1U
#define FLIPIRFREQ_MAX_BURST_MS 5000U
#define FLIPIRFREQ_VISIBLE_ROWS 4U
#define FLIPIRFREQ_ROW_HEIGHT 9

typedef enum {
    FlipIRFreqFieldFrequency,
    FlipIRFreqFieldDutyCycle,
    FlipIRFreqFieldBurst,
    FlipIRFreqFieldSignal,
    FlipIRFreqFieldMode,
    FlipIRFreqFieldOutput,
    FlipIRFreqFieldSend,
    FlipIRFreqFieldCount,
} FlipIRFreqField;

typedef enum {
    FlipIRFreqOutputAuto,
    FlipIRFreqOutputInternal,
    FlipIRFreqOutputExternal,
    FlipIRFreqOutputCount,
} FlipIRFreqOutput;

typedef enum {
    FlipIRFreqModeBurst,
    FlipIRFreqModeContinuous,
    FlipIRFreqModeCount,
} FlipIRFreqMode;

typedef enum {
    FlipIRFreqSignalModeCarrier,
    FlipIRFreqSignalModePulse,
    FlipIRFreqSignalModeCount,
} FlipIRFreqSignalMode;

typedef enum {
    FlipIRFreqEventTypeInput,
    FlipIRFreqEventTypeTick,
    FlipIRFreqEventTypeTxFinished,
} FlipIRFreqEventType;

typedef struct {
    FlipIRFreqEventType type;
    InputEvent input;
} FlipIRFreqEvent;

typedef struct {
    uint32_t frequency;
    uint32_t pulse_frequency_tenths;
    uint8_t duty_cycle;
    uint16_t burst_ms;
    uint8_t output_mode;
    uint8_t tx_mode;
    uint8_t signal_mode;
} FlipIRFreqSettings;

typedef struct {
    bool continuous;
    uint32_t remaining_us;
    uint32_t chunk_us;
} FlipIRFreqTxContext;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriTimer* ui_timer;

    uint32_t frequency;
    uint32_t pulse_frequency_tenths;
    uint8_t duty_cycle;
    uint16_t burst_ms;
    FlipIRFreqField selected_field;
    uint8_t scroll_offset;
    FlipIRFreqOutput output_mode;
    FlipIRFreqMode tx_mode;
    FlipIRFreqSignalMode signal_mode;

    bool running;
    bool transmitting;
    bool settings_dirty;
    uint32_t last_settings_change_tick;
    uint32_t tx_started_tick;
    uint8_t tx_anim_phase;
    FlipIRFreqTxContext tx_context;
    FuriTimer* pulse_timer;
    uint32_t pulse_on_ticks;
    uint32_t pulse_off_ticks;
    uint32_t pulse_remaining_ticks;
    bool pulse_level;
    const GpioPin* pulse_pin;
    char status[16];
} FlipIRFreqApp;

static void flipirfreq_ensure_selection_visible(FlipIRFreqApp* app);

static uint32_t flipirfreq_clamp_i32_to_u32(int32_t value, uint32_t min, uint32_t max) {
    if(value < (int32_t)min) return min;
    if((uint32_t)value > max) return max;
    return (uint32_t)value;
}

static uint8_t flipirfreq_clamp_i32_to_u8(int32_t value, uint8_t min, uint8_t max) {
    if(value < min) return min;
    if(value > max) return max;
    return (uint8_t)value;
}

static const char* flipirfreq_output_short_label(FuriHalInfraredTxPin output) {
    switch(output) {
    case FuriHalInfraredTxPinInternal:
        return "INT";
    case FuriHalInfraredTxPinExtPA7:
        return "EXT";
    default:
        return "?";
    }
}

static const char* flipirfreq_output_mode_label(FlipIRFreqOutput mode) {
    switch(mode) {
    case FlipIRFreqOutputAuto:
        return "AUTO";
    case FlipIRFreqOutputInternal:
        return "INT";
    case FlipIRFreqOutputExternal:
        return "EXT";
    default:
        return "?";
    }
}

static const char* flipirfreq_mode_label(FlipIRFreqMode mode) {
    switch(mode) {
    case FlipIRFreqModeBurst:
        return "BURST";
    case FlipIRFreqModeContinuous:
        return "CONT";
    default:
        return "?";
    }
}

static const char* flipirfreq_signal_mode_label(FlipIRFreqSignalMode mode) {
    switch(mode) {
    case FlipIRFreqSignalModeCarrier:
        return "CARR";
    case FlipIRFreqSignalModePulse:
        return "PULSE";
    default:
        return "?";
    }
}

static FuriHalInfraredTxPin flipirfreq_resolve_output(FlipIRFreqApp* app) {
    if(app->output_mode == FlipIRFreqOutputInternal) {
        return FuriHalInfraredTxPinInternal;
    } else if(app->output_mode == FlipIRFreqOutputExternal) {
        return FuriHalInfraredTxPinExtPA7;
    } else {
        return furi_hal_infrared_detect_tx_output();
    }
}

static const GpioPin* flipirfreq_resolve_output_gpio(FuriHalInfraredTxPin output) {
    switch(output) {
    case FuriHalInfraredTxPinInternal:
        return &gpio_infrared_tx;
    case FuriHalInfraredTxPinExtPA7:
        return &gpio_ext_pa7;
    default:
        return NULL;
    }
}

static uint32_t flipirfreq_frequency_min(FlipIRFreqApp* app) {
    return (app->signal_mode == FlipIRFreqSignalModePulse) ? FLIPIRFREQ_PULSE_MIN_FREQUENCY_TENTHS :
                                                             INFRARED_MIN_FREQUENCY;
}

static uint32_t flipirfreq_frequency_max(FlipIRFreqApp* app) {
    return (app->signal_mode == FlipIRFreqSignalModePulse) ? FLIPIRFREQ_PULSE_MAX_FREQUENCY_TENTHS :
                                                             INFRARED_MAX_FREQUENCY;
}

static uint32_t flipirfreq_frequency_step(bool coarse) {
    return coarse ? 1000U : 100U;
}

static uint32_t flipirfreq_frequency_step_for_mode(FlipIRFreqApp* app, bool coarse) {
    if(app->signal_mode == FlipIRFreqSignalModePulse) {
        return coarse ? 10U : 1U;
    }

    return flipirfreq_frequency_step(coarse);
}

static uint16_t flipirfreq_burst_step(bool coarse) {
    return coarse ? 100U : 10U;
}

static void flipirfreq_set_status(FlipIRFreqApp* app, const char* text) {
    snprintf(app->status, sizeof(app->status), "%s", text);
}

static void flipirfreq_mark_settings_dirty(FlipIRFreqApp* app) {
    app->settings_dirty = true;
    app->last_settings_change_tick = furi_get_tick();
}

static void flipirfreq_load_defaults(FlipIRFreqApp* app) {
    app->frequency = FLIPIRFREQ_DEFAULT_FREQUENCY;
    app->pulse_frequency_tenths = 380U;
    app->duty_cycle = FLIPIRFREQ_DEFAULT_DUTY_CYCLE;
    app->burst_ms = FLIPIRFREQ_DEFAULT_BURST_MS;
    app->output_mode = FlipIRFreqOutputAuto;
    app->tx_mode = FlipIRFreqModeBurst;
    app->signal_mode = FLIPIRFREQ_DEFAULT_SIGNAL_MODE;
}

static void flipirfreq_save_settings(FlipIRFreqApp* app) {
    FlipIRFreqSettings settings = {
        .frequency = app->frequency,
        .pulse_frequency_tenths = app->pulse_frequency_tenths,
        .duty_cycle = app->duty_cycle,
        .burst_ms = app->burst_ms,
        .output_mode = app->output_mode,
        .tx_mode = app->tx_mode,
        .signal_mode = app->signal_mode,
    };

    if(saved_struct_save(
           FLIPIRFREQ_SETTINGS_PATH,
           &settings,
           sizeof(settings),
           FLIPIRFREQ_SETTINGS_MAGIC,
           FLIPIRFREQ_SETTINGS_VERSION)) {
        app->settings_dirty = false;
    }
}

static void flipirfreq_load_settings(FlipIRFreqApp* app) {
    FlipIRFreqSettings settings = {0};
    flipirfreq_load_defaults(app);

    if(saved_struct_load(
           FLIPIRFREQ_SETTINGS_PATH,
           &settings,
           sizeof(settings),
           FLIPIRFREQ_SETTINGS_MAGIC,
           FLIPIRFREQ_SETTINGS_VERSION)) {
        app->duty_cycle = flipirfreq_clamp_i32_to_u8(
            settings.duty_cycle, FLIPIRFREQ_MIN_DUTY_CYCLE, FLIPIRFREQ_MAX_DUTY_CYCLE);
        app->burst_ms = flipirfreq_clamp_i32_to_u32(
            settings.burst_ms, FLIPIRFREQ_MIN_BURST_MS, FLIPIRFREQ_MAX_BURST_MS);
        app->output_mode = (settings.output_mode < FlipIRFreqOutputCount) ?
                               (FlipIRFreqOutput)settings.output_mode :
                               FlipIRFreqOutputAuto;
        app->tx_mode = (settings.tx_mode < FlipIRFreqModeCount) ?
                           (FlipIRFreqMode)settings.tx_mode :
                           FlipIRFreqModeBurst;
        app->signal_mode = (settings.signal_mode < FlipIRFreqSignalModeCount) ?
                               (FlipIRFreqSignalMode)settings.signal_mode :
                               FLIPIRFREQ_DEFAULT_SIGNAL_MODE;
        app->frequency =
            flipirfreq_clamp_i32_to_u32(settings.frequency, INFRARED_MIN_FREQUENCY, INFRARED_MAX_FREQUENCY);
        app->pulse_frequency_tenths = flipirfreq_clamp_i32_to_u32(
            settings.pulse_frequency_tenths,
            FLIPIRFREQ_PULSE_MIN_FREQUENCY_TENTHS,
            FLIPIRFREQ_PULSE_MAX_FREQUENCY_TENTHS);
    }
}

static uint32_t flipirfreq_get_active_frequency(FlipIRFreqApp* app) {
    return (app->signal_mode == FlipIRFreqSignalModePulse) ? app->pulse_frequency_tenths : app->frequency;
}

static void flipirfreq_set_active_frequency(FlipIRFreqApp* app, uint32_t value) {
    if(app->signal_mode == FlipIRFreqSignalModePulse) {
        app->pulse_frequency_tenths = value;
    } else {
        app->frequency = value;
    }
}

static FuriHalInfraredTxGetDataState
    flipirfreq_tx_data_callback(void* context, uint32_t* duration, bool* level) {
    FlipIRFreqTxContext* tx_context = context;
    *level = true;

    if(tx_context->continuous) {
        *duration = tx_context->chunk_us;
        return FuriHalInfraredTxGetDataStateDone;
    }

    if(tx_context->remaining_us > tx_context->chunk_us) {
        tx_context->remaining_us -= tx_context->chunk_us;
        *duration = tx_context->chunk_us;
        return FuriHalInfraredTxGetDataStateDone;
    }

    *duration = tx_context->remaining_us;
    tx_context->remaining_us = 0;
    return FuriHalInfraredTxGetDataStateLastDone;
}

static void flipirfreq_tx_finished_callback(void* context) {
    FlipIRFreqApp* app = context;
    FlipIRFreqEvent event = {.type = FlipIRFreqEventTypeTxFinished};
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void flipirfreq_pulse_release_pin(FlipIRFreqApp* app) {
    if(app->pulse_pin) {
        furi_hal_gpio_write(app->pulse_pin, false);
        furi_hal_gpio_init(app->pulse_pin, GpioModeAnalog, GpioPullDown, GpioSpeedLow);
        app->pulse_pin = NULL;
    }
}

static uint32_t flipirfreq_pulse_period_ticks(uint32_t frequency_tenths) {
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    uint32_t ticks = ((tick_hz * 10U) + (frequency_tenths / 2U)) / frequency_tenths;
    return (ticks < 2U) ? 2U : ticks;
}

static void flipirfreq_pulse_timer_callback(void* context) {
    FlipIRFreqApp* app = context;

    if(!app->transmitting || (app->signal_mode != FlipIRFreqSignalModePulse)) {
        return;
    }

    if((app->tx_mode == FlipIRFreqModeBurst) && (app->pulse_remaining_ticks == 0U)) {
        flipirfreq_tx_finished_callback(app);
        return;
    }

    app->pulse_level = !app->pulse_level;
    furi_hal_gpio_write(app->pulse_pin, app->pulse_level);

    uint32_t next_ticks = app->pulse_level ? app->pulse_on_ticks : app->pulse_off_ticks;
    if(app->tx_mode == FlipIRFreqModeBurst) {
        if(app->pulse_remaining_ticks <= next_ticks) {
            next_ticks = app->pulse_remaining_ticks;
            app->pulse_remaining_ticks = 0U;
        } else {
            app->pulse_remaining_ticks -= next_ticks;
        }
    }

    if(next_ticks == 0U) {
        flipirfreq_tx_finished_callback(app);
        return;
    }

    furi_timer_start(app->pulse_timer, next_ticks);
}

static void flipirfreq_stop_transmit(FlipIRFreqApp* app, bool interrupted) {
    if(!app->transmitting) {
        return;
    }

    if(app->signal_mode == FlipIRFreqSignalModePulse) {
        furi_timer_stop(app->pulse_timer);
        flipirfreq_pulse_release_pin(app);
    } else {
        furi_hal_infrared_async_tx_stop();
        furi_hal_infrared_async_tx_set_signal_sent_isr_callback(NULL, NULL);
        furi_hal_infrared_async_tx_set_data_isr_callback(NULL, NULL);
    }

    app->transmitting = false;
    app->tx_anim_phase = 0;
    app->pulse_level = false;
    app->pulse_remaining_ticks = 0;

    if(interrupted) {
        flipirfreq_set_status(app, "stopped");
    } else if(app->tx_mode == FlipIRFreqModeContinuous) {
        flipirfreq_set_status(app, "DONE");
    } else {
        flipirfreq_set_status(app, "DONE");
    }
}

static void flipirfreq_start_transmit(FlipIRFreqApp* app) {
    if(furi_hal_infrared_is_busy()) {
        flipirfreq_set_status(app, "BUSY");
        return;
    }

    const FuriHalInfraredTxPin output = flipirfreq_resolve_output(app);
    if(app->signal_mode == FlipIRFreqSignalModePulse) {
        const GpioPin* gpio = flipirfreq_resolve_output_gpio(output);
        const uint32_t period_ticks = flipirfreq_pulse_period_ticks(app->pulse_frequency_tenths);
        uint32_t on_ticks = (period_ticks * app->duty_cycle) / 100U;
        if(on_ticks == 0U) on_ticks = 1U;
        if(on_ticks >= period_ticks) on_ticks = period_ticks - 1U;

        app->pulse_pin = gpio;
        app->pulse_on_ticks = on_ticks;
        app->pulse_off_ticks = period_ticks - on_ticks;
        app->pulse_remaining_ticks =
            (app->tx_mode == FlipIRFreqModeBurst) ? furi_ms_to_ticks(app->burst_ms) : 0U;
        app->pulse_level = true;

        furi_hal_gpio_write(gpio, false);
        furi_hal_gpio_init(gpio, GpioModeOutputPushPull, GpioPullDown, GpioSpeedHigh);
        furi_hal_gpio_write(gpio, true);

        if(app->tx_mode == FlipIRFreqModeBurst) {
            if(app->pulse_remaining_ticks <= app->pulse_on_ticks) {
                app->pulse_on_ticks = app->pulse_remaining_ticks;
                app->pulse_remaining_ticks = 0U;
            } else {
                app->pulse_remaining_ticks -= app->pulse_on_ticks;
            }
        }

        furi_timer_start(app->pulse_timer, app->pulse_on_ticks);
    } else {
        app->tx_context = (FlipIRFreqTxContext){
            .continuous = (app->tx_mode == FlipIRFreqModeContinuous),
            .remaining_us = (uint32_t)app->burst_ms * 1000U,
            .chunk_us = FLIPIRFREQ_TX_CHUNK_US,
        };

        furi_hal_infrared_set_tx_output(output);
        furi_hal_infrared_async_tx_set_data_isr_callback(
            flipirfreq_tx_data_callback, &app->tx_context);
        furi_hal_infrared_async_tx_set_signal_sent_isr_callback(
            (app->tx_mode == FlipIRFreqModeBurst) ? flipirfreq_tx_finished_callback : NULL,
            app);
        furi_hal_infrared_async_tx_start(app->frequency, (float)app->duty_cycle / 100.0f);
    }

    app->transmitting = true;
    app->tx_started_tick = furi_get_tick();
    app->tx_anim_phase = 0;
    app->selected_field = FlipIRFreqFieldSend;
    flipirfreq_ensure_selection_visible(app);

    flipirfreq_set_status(app, "LIVE");
}

static void flipirfreq_adjust_field(FlipIRFreqApp* app, bool increase, bool coarse) {
    const int32_t direction = increase ? 1 : -1;

    switch(app->selected_field) {
    case FlipIRFreqFieldFrequency: {
        int32_t next = (int32_t)flipirfreq_get_active_frequency(app) +
                       (int32_t)flipirfreq_frequency_step_for_mode(app, coarse) * direction;
        flipirfreq_set_active_frequency(
            app, flipirfreq_clamp_i32_to_u32(next, flipirfreq_frequency_min(app), flipirfreq_frequency_max(app)));
        break;
    }
    case FlipIRFreqFieldDutyCycle: {
        int32_t next = (int32_t)app->duty_cycle + (coarse ? 5 : 1) * direction;
        app->duty_cycle =
            flipirfreq_clamp_i32_to_u8(next, FLIPIRFREQ_MIN_DUTY_CYCLE, FLIPIRFREQ_MAX_DUTY_CYCLE);
        break;
    }
    case FlipIRFreqFieldBurst: {
        int32_t next = (int32_t)app->burst_ms + (int32_t)flipirfreq_burst_step(coarse) * direction;
        app->burst_ms =
            flipirfreq_clamp_i32_to_u32(next, FLIPIRFREQ_MIN_BURST_MS, FLIPIRFREQ_MAX_BURST_MS);
        break;
    }
    case FlipIRFreqFieldSignal: {
        int32_t next = (int32_t)app->signal_mode + direction;
        if(next < 0) {
            next = FlipIRFreqSignalModeCount - 1;
        } else if(next >= FlipIRFreqSignalModeCount) {
            next = 0;
        }
        app->signal_mode = next;
        break;
    }
    case FlipIRFreqFieldMode: {
        int32_t next = (int32_t)app->tx_mode + direction;
        if(next < 0) {
            next = FlipIRFreqModeCount - 1;
        } else if(next >= FlipIRFreqModeCount) {
            next = 0;
        }
        app->tx_mode = next;
        break;
    }
    case FlipIRFreqFieldOutput: {
        int32_t next = (int32_t)app->output_mode + direction;
        if(next < 0) {
            next = FlipIRFreqOutputCount - 1;
        } else if(next >= FlipIRFreqOutputCount) {
            next = 0;
        }
        app->output_mode = next;
        break;
    }
    default:
        break;
    }
}

static const char* flipirfreq_tx_indicator(uint8_t phase) {
    static const char* frames[] = {"TX", "TX.", "TX..", "TX..."};
    return frames[phase % COUNT_OF(frames)];
}

static void flipirfreq_ensure_selection_visible(FlipIRFreqApp* app) {
    if(app->selected_field < app->scroll_offset) {
        app->scroll_offset = app->selected_field;
    } else if(app->selected_field >= (app->scroll_offset + FLIPIRFREQ_VISIBLE_ROWS)) {
        app->scroll_offset = app->selected_field - FLIPIRFREQ_VISIBLE_ROWS + 1U;
    }
}

static const char* flipirfreq_field_label(FlipIRFreqField field) {
    switch(field) {
    case FlipIRFreqFieldFrequency:
        return "Frequency";
    case FlipIRFreqFieldDutyCycle:
        return "Duty Cycle";
    case FlipIRFreqFieldBurst:
        return "Burst Time";
    case FlipIRFreqFieldSignal:
        return "Signal Type";
    case FlipIRFreqFieldMode:
        return "Tx Mode";
    case FlipIRFreqFieldOutput:
        return "IR Output";
    case FlipIRFreqFieldSend:
        return "Transmit";
    default:
        return "?";
    }
}

static void flipirfreq_field_value(FlipIRFreqApp* app, FlipIRFreqField field, char* value, size_t size) {
    switch(field) {
    case FlipIRFreqFieldFrequency:
        if(app->signal_mode == FlipIRFreqSignalModePulse) {
            snprintf(
                value,
                size,
                "%lu.%lu Hz",
                (unsigned long)(app->pulse_frequency_tenths / 10U),
                (unsigned long)(app->pulse_frequency_tenths % 10U));
        } else {
            snprintf(value, size, "%lu Hz", (unsigned long)app->frequency);
        }
        break;
    case FlipIRFreqFieldDutyCycle:
        snprintf(value, size, "%u%%", app->duty_cycle);
        break;
    case FlipIRFreqFieldBurst:
        snprintf(value, size, "%u ms", (unsigned int)app->burst_ms);
        break;
    case FlipIRFreqFieldSignal:
        snprintf(value, size, "%s", flipirfreq_signal_mode_label(app->signal_mode));
        break;
    case FlipIRFreqFieldMode:
        snprintf(value, size, "%s", flipirfreq_mode_label(app->tx_mode));
        break;
    case FlipIRFreqFieldOutput:
        if(app->output_mode == FlipIRFreqOutputAuto) {
            snprintf(
                value,
                size,
                "AUTO>%s",
                flipirfreq_output_short_label(flipirfreq_resolve_output(app)));
        } else {
            snprintf(value, size, "%s", flipirfreq_output_mode_label(app->output_mode));
        }
        break;
    case FlipIRFreqFieldSend:
        snprintf(
            value,
            size,
            "%s",
            app->transmitting ? "STOP" : (app->tx_mode == FlipIRFreqModeContinuous ? "START" : "SEND"));
        break;
    default:
        value[0] = '\0';
        break;
    }
}

static void flipirfreq_draw_row(
    Canvas* canvas,
    int32_t y,
    bool selected,
    const char* label,
    const char* value) {
    if(selected) {
        canvas_draw_box(canvas, 0, y - 7, 128, 9);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_str(canvas, 3, y, label);
    canvas_draw_str_aligned(canvas, 125, y, AlignRight, AlignBottom, value);

    if(selected) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void flipirfreq_draw_callback(Canvas* canvas, void* context) {
    FlipIRFreqApp* app = context;
    char value[32];

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "FlipIRFreq");
    if(app->transmitting) {
        canvas_draw_str_aligned(
            canvas, 125, 9, AlignRight, AlignBottom, flipirfreq_tx_indicator(app->tx_anim_phase));
    } else if(app->status[0] != '\0') {
        canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, app->status);
    }

    canvas_set_font(canvas, FontSecondary);

    for(uint8_t row = 0; row < FLIPIRFREQ_VISIBLE_ROWS; row++) {
        const uint8_t field_index = app->scroll_offset + row;
        if(field_index >= FlipIRFreqFieldCount) break;

        const FlipIRFreqField field = (FlipIRFreqField)field_index;
        flipirfreq_field_value(app, field, value, sizeof(value));
        flipirfreq_draw_row(
            canvas,
            18 + (row * FLIPIRFREQ_ROW_HEIGHT),
            app->selected_field == field,
            flipirfreq_field_label(field),
            value);
    }

    if(app->scroll_offset > 0) {
        canvas_draw_str(canvas, 120, 16, "^");
    }
    if((app->scroll_offset + FLIPIRFREQ_VISIBLE_ROWS) < FlipIRFreqFieldCount) {
        canvas_draw_str(canvas, 120, 53, "v");
    }

    canvas_draw_line(canvas, 2, 54, 126, 54);
    canvas_draw_str(canvas, 2, 62, "By ConsultingJoe.com");
}

static void flipirfreq_input_callback(InputEvent* input_event, void* context) {
    FlipIRFreqApp* app = context;
    FlipIRFreqEvent event = {.type = FlipIRFreqEventTypeInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void flipirfreq_handle_input(FlipIRFreqApp* app, const InputEvent* input) {
    const bool pressed = (input->type == InputTypeShort) || (input->type == InputTypeRepeat);
    const bool coarse = input->type == InputTypeRepeat;

    if(input->key == InputKeyBack && input->type == InputTypeShort) {
        if(app->transmitting) {
            flipirfreq_stop_transmit(app, true);
            return;
        }
        app->running = false;
        return;
    }

    if(app->transmitting) {
        if((input->key == InputKeyOk) && (input->type == InputTypeShort)) {
            flipirfreq_stop_transmit(app, true);
        }
        return;
    }

    if(!pressed && input->key != InputKeyOk) {
        return;
    }

    switch(input->key) {
    case InputKeyUp:
        if(pressed) {
            if(app->selected_field == 0) {
                app->selected_field = FlipIRFreqFieldCount - 1;
            } else {
                app->selected_field--;
            }
            flipirfreq_ensure_selection_visible(app);
        }
        break;
    case InputKeyDown:
        if(pressed) {
            app->selected_field = (app->selected_field + 1) % FlipIRFreqFieldCount;
            flipirfreq_ensure_selection_visible(app);
        }
        break;
    case InputKeyLeft:
        if(pressed) {
            flipirfreq_adjust_field(app, false, coarse);
            if(app->selected_field != FlipIRFreqFieldSend) {
                flipirfreq_mark_settings_dirty(app);
            }
        }
        break;
    case InputKeyRight:
        if(pressed) {
            flipirfreq_adjust_field(app, true, coarse);
            if(app->selected_field != FlipIRFreqFieldSend) {
                flipirfreq_mark_settings_dirty(app);
            }
        }
        break;
    case InputKeyOk:
        if(input->type == InputTypeShort) {
            if(app->selected_field == FlipIRFreqFieldSend) {
                if(app->transmitting) {
                    flipirfreq_stop_transmit(app, true);
                } else {
                    flipirfreq_start_transmit(app);
                }
            }
        }
        break;
    default:
        break;
    }
}

static void flipirfreq_tick_callback(void* context) {
    FlipIRFreqApp* app = context;
    FlipIRFreqEvent event = {.type = FlipIRFreqEventTypeTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void flipirfreq_handle_tick(FlipIRFreqApp* app) {
    if(app->transmitting) {
        app->tx_anim_phase = (app->tx_anim_phase + 1) & 0x03;
    }

    if(app->settings_dirty &&
       ((furi_get_tick() - app->last_settings_change_tick) >= (furi_kernel_get_tick_frequency() / 2))) {
        flipirfreq_save_settings(app);
    }
}

static FlipIRFreqApp* flipirfreq_app_alloc(void) {
    FlipIRFreqApp* app = malloc(sizeof(FlipIRFreqApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(FlipIRFreqEvent));
    app->ui_timer = furi_timer_alloc(flipirfreq_tick_callback, FuriTimerTypePeriodic, app);
    app->pulse_timer = furi_timer_alloc(flipirfreq_pulse_timer_callback, FuriTimerTypeOnce, app);

    flipirfreq_load_settings(app);
    app->selected_field = FlipIRFreqFieldFrequency;
    app->scroll_offset = 0;
    app->running = true;
    app->transmitting = false;
    app->settings_dirty = false;
    app->last_settings_change_tick = 0;
    app->tx_started_tick = 0;
    app->tx_anim_phase = 0;
    app->pulse_on_ticks = 0;
    app->pulse_off_ticks = 0;
    app->pulse_remaining_ticks = 0;
    app->pulse_level = false;
    app->pulse_pin = NULL;
    flipirfreq_set_status(app, "");

    view_port_draw_callback_set(app->view_port, flipirfreq_draw_callback, app);
    view_port_input_callback_set(app->view_port, flipirfreq_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    furi_timer_start(app->ui_timer, furi_kernel_get_tick_frequency() / FLIPIRFREQ_UI_TICK_HZ);

    return app;
}

static void flipirfreq_app_free(FlipIRFreqApp* app) {
    if(!app) return;

    if(app->transmitting) {
        flipirfreq_stop_transmit(app, true);
    }
    if(app->settings_dirty) {
        flipirfreq_save_settings(app);
    }
    furi_timer_stop(app->pulse_timer);
    furi_timer_free(app->pulse_timer);
    furi_timer_stop(app->ui_timer);
    furi_timer_free(app->ui_timer);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t flipirfreq_app(void* p) {
    UNUSED(p);

    FlipIRFreqApp* app = flipirfreq_app_alloc();
    FlipIRFreqEvent event;

    view_port_update(app->view_port);

    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == FlipIRFreqEventTypeInput) {
                flipirfreq_handle_input(app, &event.input);
            } else if(event.type == FlipIRFreqEventTypeTick) {
                flipirfreq_handle_tick(app);
            } else if(event.type == FlipIRFreqEventTypeTxFinished) {
                if(app->tx_mode == FlipIRFreqModeBurst) {
                    flipirfreq_stop_transmit(app, false);
                }
            }
            view_port_update(app->view_port);
        }
    }

    flipirfreq_app_free(app);
    return 0;
}
