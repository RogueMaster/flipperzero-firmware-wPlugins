/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401DigiLab_probe.h"
//static const char* TAG = "401_DigiLabProbe";

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <limits.h>
#include <stm32wbxx.h>
#include <stm32wbxx_ll_tim.h>

static uint32_t lastTime;

static void probe_adc_start(FuriHalAdcHandle** adc) {
    *adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(*adc);
}

static void probe_adc_stop(FuriHalAdcHandle** adc) {
    if(*adc != NULL) {
        furi_hal_adc_release(*adc);
    }
}

#define SCALE_DIV 10

void intfall_callback(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppProbe* appProbe = (AppProbe*)app->sceneProbe;
    ProbeModel* model = view_get_model(appProbe->view);
    // Capture the current cycle count from the Data Watchpoint and Trace (DWT)
    // unit
    uint32_t currentTime = DWT->CYCCNT;
    uint32_t period;
    // Calculate the period, accounting for potential counter wrap-around
    if(currentTime >= lastTime) {
        period = currentTime - lastTime;
    } else {
        period = (0xFFFFFFFF - lastTime + 1) + currentTime;
    }

    // Update the lastTime to the current time
    lastTime = currentTime;
    RingBuffer_add(model->rb, period * SCALE_DIV);
    // Commit the model view update without triggering a full redraw
    view_commit_model(appProbe->view, false);
}

void probe_timer_cb(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppProbe* appProbe = (AppProbe*)app->sceneProbe;
    ProbeModel* model = view_get_model(appProbe->view);

    uint32_t SystemCoreClock = furi_hal_cortex_instructions_per_microsecond() * 1000000U;
    model->period = (double)((double)RingBuffer_getAverage(model->rb) / SCALE_DIV);
    model->frequency = (double)((double)SystemCoreClock / (double)model->period);
    model->inf = isinf(model->frequency);
    model->raw = furi_hal_adc_read(appProbe->adc, FuriHalAdcChannel4);
    model->voltage = (double)((double)furi_hal_adc_convert_to_voltage(appProbe->adc, model->raw) *
                              (model->BridgeFactor / 10));
    view_commit_model(appProbe->view, true);
    RingBuffer_reset(model->rb);
}

void counter_start(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;

    // Capture the current value of the DWT cycle counter (for timing
    // calculations)
    lastTime = DWT->CYCCNT;
    // Configure the TIM2 timer to count upwards (normal counting mode)
    LL_TIM_SetCounterMode(TIM2, LL_TIM_COUNTERMODE_UP);
    // Set the clock division for TIM2 (no division, running at full clock speed)
    LL_TIM_SetClockDivision(TIM2, LL_TIM_CLOCKDIVISION_DIV1);
    // Set the prescaler to 0, meaning the timer will run at the system clock
    // frequency
    LL_TIM_SetPrescaler(TIM2, 0);
    // Set the auto-reload value to its maximum (0xFFFFFFFF), allowing the counter
    // to count up to the maximum
    LL_TIM_SetAutoReload(TIM2, 0xFFFFFFFF);
    // Initialize GPIO pin for interrupt (falling edge) with high speed and
    // pull-down configuration
    furi_hal_gpio_init(
        L401DIGILAB_BOARD_PIN_INT, GpioModeInterruptFall, GpioPullDown, GpioSpeedVeryHigh);
    // Attach an interrupt callback function to the GPIO pin (for handling falling
    // edge interrupts)
    furi_hal_gpio_add_int_callback(L401DIGILAB_BOARD_PIN_INT, intfall_callback, app);
    // Initialize the TIM2 counter value to 0 (reset the counter)
    LL_TIM_SetCounter(TIM2, 0);
    // Start the TIM2 counter (begin counting)
    LL_TIM_EnableCounter(TIM2);
}

static void probe_counter_stop() {
    // Disable the TIM2 counter (stops the counting process)
    LL_TIM_DisableCounter(TIM2);
    // Disable the TIM2 update interrupt (prevents further timer interrupts)
    LL_TIM_DisableIT_UPDATE(TIM2);
    // Remove the interrupt callback for the GPIO pin
    furi_hal_gpio_remove_int_callback(L401DIGILAB_BOARD_PIN_INT);
    // Optionally, clear the TIM2 counter value (if resetting is needed)
    LL_TIM_SetCounter(TIM2, 0);
}

void probe_draw_ladder(Canvas* canvas, ProbeModel* model) {
    double voltage = model->voltage;

    // Clamp voltage
    if(voltage < 0) voltage = 0;
    if(voltage > (double)BARGRAPH_MAX_VOLTAGE) voltage = (double)BARGRAPH_MAX_VOLTAGE;

    // Draw outer rounded frame
    canvas_draw_rframe(canvas, 0, 0, BARGRAPH_WIDTH, BARGRAPH_HEIGHT, BARGRAPH_MARGIN);

    // Define inner drawable area (excluding margins)
    const uint8_t x_min = BARGRAPH_MARGIN;
    const uint8_t x_max = BARGRAPH_WIDTH - BARGRAPH_MARGIN - 1;
    const uint8_t y_min = BARGRAPH_MARGIN;
    const uint8_t y_max = BARGRAPH_HEIGHT - BARGRAPH_MARGIN - 1;

    const uint8_t inner_height = y_max - y_min + 1;

    // Calculate bar height based on clamped voltage
    uint8_t bar_height = (uint8_t)((voltage / (double)BARGRAPH_MAX_VOLTAGE) * inner_height);

    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
    canvas_draw_str_aligned(canvas, 10, BARGRAPH_3vLabel, AlignLeft, AlignTop, "3v");
    canvas_draw_str_aligned(canvas, 10, BARGRAPH_5vLabel, AlignLeft, AlignTop, "5v");
    canvas_draw_str_aligned(canvas, 10, BARGRAPH_12vLabel, AlignLeft, AlignTop, "12v");

    if(bar_height == 0) return; // Nothing to draw
    const uint8_t bar_width = x_max - x_min; // leave 1px margin on right
    canvas_draw_box(canvas, x_min, y_max - bar_height + 1, x_min + bar_width - 1, y_max);
}

void probe_draw_screen(Canvas* canvas, ProbeModel* model) {
    furi_check(model);
    UNUSED(canvas);
    char str[128] = {0};
    double voltage = model->voltage;
    snprintf(str, 128, "%2.2fv", voltage);

    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_HUGE_REGULAR);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, str);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Probe");

    if(!model->inf) {
        snprintf(str, 128, "%0.2fHz", model->frequency);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, str);

    } else {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Continuous");
    }

    probe_draw_ladder(canvas, model);
    canvas_set_font(canvas, FontSecondary);
    elements_button_right(canvas, "Scope");
}

/**
 * @brief Render callback for the app probe.
 *
 * @param canvas The canvas to be used for rendering.
 * @param model The model passed to the callback.
 */
void app_probe_render_callback(Canvas* canvas, void* ctx) {
    furi_check(ctx);
    furi_check(canvas);
    ProbeModel* model = (ProbeModel*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    probe_draw_screen(canvas, model);
}

/**
 * @brief Allocate memory and initialize the app probe.
 *
 * @return Returns a pointer to the allocated AppProbe.
 */
AppProbe* app_probe_alloc(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppProbe* appProbe = malloc(sizeof(AppProbe));
    appProbe->timer = furi_timer_alloc(probe_timer_cb, FuriTimerTypePeriodic, app);
    appProbe->view = view_alloc();
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    view_allocate_model(appProbe->view, ViewModelTypeLockFree, sizeof(ProbeModel));
    with_view_model(
        appProbe->view,
        ProbeModel * model,
        {
            model->rb = RingBuffer_create(10);
            model->frequency = 0;
            model->BridgeFactor = (app->data->config->BridgeFactor / 1000);
        },
        false);

    furi_hal_bus_enable(FuriHalBusTIM2);
    view_set_context(appProbe->view, app);
    view_set_input_callback(appProbe->view, app_probe_input_callback);
    view_set_draw_callback(appProbe->view, app_probe_render_callback);
    return appProbe;
}

/**
 * @brief Callback function to handle input events for the app probe.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_probe_input_callback(InputEvent* input_event, void* ctx) {
    furi_check(ctx);
    furi_check(input_event);
    AppContext* app = (AppContext*)ctx;
    bool handled = false;

    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyUp:
            break;
        case InputKeyDown:
            break;
        case InputKeyLeft:
            break;
        case InputKeyRight:
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, AppSceneScope);
            break;
        default:
            break;
        }
    }
    return handled;
}

/**
 * @brief Get the view associated with the app probe.
 *
 * @param appProbe The AppProbe for which the view is to be fetched.
 * @return Returns a pointer to the View.
 */
View* app_probe_get_view(AppProbe* appProbe) {
    furi_check(appProbe);
    return appProbe->view;
}

/**
 * @brief Callback when the app probe scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_probe_on_enter(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppProbe* appProbe = (AppProbe*)app->sceneProbe;
    ProbeModel* model = view_get_model(appProbe->view);

    //Reload latest Bridge Factor
    model->BridgeFactor = (app->data->config->BridgeFactor / 100);
    probe_adc_start(&appProbe->adc);
    counter_start(app);
    furi_timer_start(appProbe->timer, (furi_kernel_get_tick_frequency() / 5));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewProbe);
}

/**
 * @brief Handle scene manager events for the app probe scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_probe_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
 * @brief Callback when the app probe scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_probe_on_exit(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppProbe* appProbe = (AppProbe*)app->sceneProbe;
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    probe_adc_stop(&appProbe->adc);
    probe_counter_stop();
    furi_timer_stop(appProbe->timer);
}

/**
 * @brief Free the memory occupied by the app probe.
 *
 * @param appProbe The AppProbe to be freed.
 */
void app_probe_free(AppProbe* appProbe) {
    furi_check(appProbe);
    with_view_model(appProbe->view, ProbeModel * model, { RingBuffer_free(model->rb); }, false);
    furi_timer_free(appProbe->timer);
    furi_hal_bus_disable(FuriHalBusTIM2);
    view_free_model(appProbe->view);
    view_free(appProbe->view);
    free(appProbe);
}
