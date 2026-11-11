/*
 * GPIO Frequency Counter
 *
 * Counts rising edges of an external digital signal on header pin PB3 using
 * TIM2 configured in external-clock counting mode 1 (the counter is clocked by
 * the filtered timer input TI2FP2, i.e. edges are counted in silicon). A normal
 * ~1s FuriTimer running in the app thread samples the free-running counter and
 * derives Hz from the delta. This deliberately avoids per-edge EXTI interrupts,
 * which could starve the M4 at high input frequencies.
 *
 * Timer bus hygiene: TIM2 is claimed with furi_hal_bus_enable() on start and
 * released with furi_hal_bus_disable() on exit; PB3 is switched to its TIM2
 * alternate function on start and back to analog on exit (mirrors the GPIO
 * Logic Monitor's on-exit cleanup). Being an external FAP, any fault only
 * returns the user to the desktop.
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>

#include <gui/gui.h>
#include <input/input.h>

#include <stm32wbxx_ll_tim.h>

#define FREQ_TIM         TIM2
#define FREQ_TIM_BUS     FuriHalBusTIM2
#define FREQ_TIM_CHANNEL LL_TIM_CHANNEL_CH2
#define FREQ_SAMPLE_MS   (1000U)

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriTimer* timer;
    FuriMutex* mutex;

    uint32_t last_count; // previous raw counter snapshot (timer thread only)
    uint32_t hz; // last measured frequency (shared, guarded by mutex)
    uint32_t total; // running total of counted edges (shared, guarded by mutex)
} GpioFreqCounter;

/* Configure TIM2 as an edge counter clocked by PB3 (TIM2_CH2 / TI2, AF1).
 * Uses inline LL_TIM setters only; LL_TIM_Init / LL_TIM_IC_Init are avoided
 * (not exported to FAPs). External clock mode 1 with trigger = TI2FP2. */
static void gpio_freq_counter_hw_start(void) {
    furi_hal_bus_enable(FREQ_TIM_BUS);

    // Route PB3 to its TIM2 alternate function (TI2 input).
    furi_hal_gpio_init_ex(
        &gpio_ext_pb3, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn1TIM2);

    // Free-running 32-bit up-counter, no prescale, max reload.
    LL_TIM_SetPrescaler(FREQ_TIM, 0);
    LL_TIM_SetCounterMode(FREQ_TIM, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(FREQ_TIM, 0xFFFFFFFFUL);
    LL_TIM_SetClockDivision(FREQ_TIM, LL_TIM_CLOCKDIVISION_DIV1);

    // Channel 2 as input mapped on TI2 (CC2S=01), no input prescale, no filter.
    LL_TIM_IC_SetActiveInput(FREQ_TIM, FREQ_TIM_CHANNEL, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(FREQ_TIM, FREQ_TIM_CHANNEL, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(FREQ_TIM, FREQ_TIM_CHANNEL, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(FREQ_TIM, FREQ_TIM_CHANNEL, LL_TIM_IC_POLARITY_RISING);

    // Clock the counter from the filtered TI2 edge (external clock mode 1).
    LL_TIM_SetTriggerInput(FREQ_TIM, LL_TIM_TS_TI2FP2);
    LL_TIM_SetClockSource(FREQ_TIM, LL_TIM_CLOCKSOURCE_EXT_MODE1);

    LL_TIM_SetCounter(FREQ_TIM, 0);
    LL_TIM_EnableCounter(FREQ_TIM);
}

static void gpio_freq_counter_hw_stop(void) {
    LL_TIM_DisableCounter(FREQ_TIM);
    furi_hal_bus_disable(FREQ_TIM_BUS);
    // Return PB3 to a safe, high-impedance state.
    furi_hal_gpio_init(&gpio_ext_pb3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static void gpio_freq_counter_timer_callback(void* ctx) {
    furi_assert(ctx);
    GpioFreqCounter* app = ctx;

    // Delta of a free-running counter; unsigned subtraction is wraparound-safe.
    uint32_t now = LL_TIM_GetCounter(FREQ_TIM);
    uint32_t delta = now - app->last_count;
    app->last_count = now;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->hz = delta; // sample window is FREQ_SAMPLE_MS ~= 1 s
    app->total += delta;
    furi_mutex_release(app->mutex);

    view_port_update(app->view_port);
}

static void gpio_freq_counter_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    GpioFreqCounter* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint32_t hz = app->hz;
    uint32_t total = app->total;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "GPIO Freq Counter");

    char buf[24];
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)hz);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "Hz  (input: pin PB3)");

    snprintf(buf, sizeof(buf), "Edges: %lu", (unsigned long)total);
    canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, buf);
}

static void gpio_freq_counter_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    GpioFreqCounter* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

int32_t gpio_freq_counter_app(void* p) {
    UNUSED(p);

    GpioFreqCounter* app = malloc(sizeof(GpioFreqCounter));
    app->last_count = 0;
    app->hz = 0;
    app->total = 0;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, gpio_freq_counter_draw_callback, app);
    view_port_input_callback_set(app->view_port, gpio_freq_counter_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    gpio_freq_counter_hw_start();

    app->timer = furi_timer_alloc(gpio_freq_counter_timer_callback, FuriTimerTypePeriodic, app);
    // furi_kernel_get_tick_frequency() == ticks per second -> ~1 s window.
    furi_timer_start(app->timer, furi_ms_to_ticks(FREQ_SAMPLE_MS));

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.key == InputKeyBack &&
               (event.type == InputTypeShort || event.type == InputTypeLong)) {
                running = false;
            }
        }
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    gpio_freq_counter_hw_stop();

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
