/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "scenes/scope/401DigiLab_scope.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_speaker.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>
#include <limits.h>
#include <401_config.h>

uint16_t redraw_cnt = 0;
#define SPEAKER_ACQUIRE_TIMEOUT 100

/**
 * @brief Stop and release the speaker if currently active.
 */
static void scope_sound_stop() {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

/**
 * @brief Turn off all LEDs on the SK6805 driver.
 */
static void scope_LED_stop() {
    SK6805_off();
}

/**
 * @brief Disable the vibration actuator.
 */
static void scope_vibro_stop() {
    furi_hal_gpio_write(&gpio_vibro, false);
}

/**
 * @brief Acquire and configure the ADC handle for sampling.
 *
 * @param[out] adc Double pointer to store the acquired ADC handle.
 */
static void scope_adc_start(FuriHalAdcHandle** adc) {
    *adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(*adc);
}

/**
 * @brief Release the previously acquired ADC handle.
 *
 * @param[in,out] adc Double pointer referencing the ADC handle to release.
 */
static void scope_adc_stop(FuriHalAdcHandle** adc) {
    if(*adc) {
        furi_hal_adc_release(*adc);
    }
}

/**
 * @brief Evaluate whether the current voltage reading triggers an alert.
 *
 * @param model Pointer to the ScopeModel containing voltage data.
 * @param ctx   Pointer to the application context for configuration access.
 * @return true if the configured alert condition is met; false otherwise.
 */
static bool CheckAlert(ScopeModel* model, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    float voltage = model->voltage / 10;
    switch(app->data->config->ScopeAlert) {
    case DigiLab_ScopeAlert_lt3V:
        return ((voltage) < 3.3);
        break;
    case DigiLab_ScopeAlert_gt3V:
        return ((voltage) > 3.3);
        break;
    case DigiLab_ScopeAlert_lt5V:
        return ((voltage) < 5.0);
        break;
    case DigiLab_ScopeAlert_gt5V:
        return ((voltage) > 5.0);
        break;
    case DigiLab_ScopeAlert_osc:
        if(model->inversion) {
            model->inversion = false;
            return true;
        }
        return false;
        break;
    case DigiLab_ScopeAlert_maxV:
        return ((voltage) > 11);
        break;
    case DigiLab_ScopeAlert_0V:
        return ((voltage) < 0.3);
        break;
    default:
        return false;
        break;
    }
}

/**
* @brief Manage audio feedback based on voltage and alert status.
*
* @param app   Pointer to the application context.
* @param model Pointer to the ScopeModel containing voltage data.
* @param alert Boolean indicating if an alert condition is active.
*/
static void adc_scopeSound(AppContext* app, ScopeModel* model, bool alert) {
    float voltage = model->voltage / 10;

    switch(app->data->config->ScopeSound) {
    case Digilab_ScopeSoundOn:
        if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(SPEAKER_ACQUIRE_TIMEOUT)) {
            furi_hal_speaker_start((float)(voltage * 75), 1.0F);
        }
        break;
    case DigiLab_ScopeSoundAlert:
        if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(SPEAKER_ACQUIRE_TIMEOUT)) {
            if(app->data->config->ScopeAlert == DigiLab_ScopeAlert_osc) {
                if(alert) {
                    furi_hal_speaker_start(model->inversion_count * 30, 1);
                } else {
                    furi_hal_speaker_start(0, 0);
                }
            } else {
                if(alert) {
                    furi_hal_speaker_start(500, 1);
                } else {
                    furi_hal_speaker_start(0, 0);
                }
            }
        }
        break;
    default:
    case DigiLab_ScopeSoundOff:
        break;
    }
}

/**
* @brief Manage vibration feedback based on alert status.
*
* @param app   Pointer to the application context.
* @param model Pointer to the ScopeModel (unused).
* @param alert Boolean indicating if an alert condition is active.
*/
static void adc_scopeVibro(AppContext* app, ScopeModel* model, bool alert) {
    UNUSED(model);
    switch(app->data->config->ScopeVibro) {
    case DigiLab_ScopeVibroAlert:
        if(alert) {
            furi_hal_gpio_write(&gpio_vibro, true);
        } else {
            furi_hal_gpio_write(&gpio_vibro, false);
        }
        break;
    default:
    case DigiLab_ScopeVibroOff:
        break;
    }
}

/**
* @brief Calculate RGB color values corresponding to a voltage level.
*
* @param voltage Voltage value, clamped to the maximum allowed.
* @param[out] r  Pointer to receive the computed red component (0–255).
* @param[out] g  Pointer to receive the computed green component (0–255).
* @param[out] b  Pointer to receive the computed blue component (0–255).
*/
static void adc_scopeLed_voltageColor(uint32_t voltage, uint8_t* r, uint8_t* g, uint8_t* b) {
    // Higher voltage limit
    const uint32_t max_voltage = 12; // Max 12V
    if(voltage > max_voltage) {
        voltage = max_voltage;
    }
    // Color ramp
    uint32_t ratio = (voltage * 255) / max_voltage;
    *r = (ratio * 255) / 255;
    *g = ((255 - ratio) * 255) / 255;
    *b = 0;
}

/**
 * @brief Update the LED strip based on voltage, variance, and alert modes.
 *
 * @param app   Pointer to the application context.
 * @param model Pointer to the ScopeModel containing waveform and voltage data.
 * @param alert Boolean indicating if an alert condition is active.
 */
static void adc_scopeLed(AppContext* app, ScopeModel* model, bool alert) {
    float voltage = model->voltage / 10;
    switch(app->data->config->ScopeLed) {
    case DigiLab_ScopeLedOff:
        SK6805_set_led_color(0, 0, 0, 0);
        SK6805_update();
        break;
    case DigiLab_ScopeLedAlert:
        if(app->data->config->ScopeAlert == DigiLab_ScopeAlert_osc) {
            if(alert) {
                SK6805_set_led_color(0, 0, 0x88, model->inversion_count * 3);
            } else {
                SK6805_set_led_color(0, 0, 0, 0);
            }
            SK6805_update();
        } else {
            if(alert) {
                SK6805_set_led_color(0, 0xFF, 0xFF, 0);
            } else {
                SK6805_set_led_color(0, 0, 0, 0);
            }
            SK6805_update();
        }
        break;
    case DigiLab_ScopeLedFollow:
        if(voltage > 0.5) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            adc_scopeLed_voltageColor(voltage, &r, &g, &b);
            SK6805_set_led_color(0, r, g, b);
            SK6805_update();
        } else {
            SK6805_set_led_color(0, 0, 0, 0);
            SK6805_update();
        }
        break;
    case DigiLab_ScopeLedVariance:
        uint8_t mappedVariance = RingBuffer_getStandardDeviationMapped(
            model->oscWindow->samples, model->oscWindow->vMin, model->oscWindow->vMax);
        SK6805_set_led_color(
            0, mappedVariance, 255 - mappedVariance, (uint8_t)(255 / model->oscWindow->vMax));
        SK6805_update();
        break;
    case DigiLab_ScopeLedTrigger:
        if(voltage > (model->oscWindow->vTrig / OSC_SCALE_V)) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            adc_scopeLed_voltageColor(voltage, &r, &g, &b);
            SK6805_set_led_color(0, r, g, b);
            SK6805_update();
        } else {
            SK6805_set_led_color(0, 0xFF, 0, 0);
            SK6805_update();
        }
        break;
    }
}

bool previous_int = false;
bool current_int = false;

/**
 * @brief Timer callback for periodic ADC sampling and redraw event dispatch.
 *
 * Reads the ADC, updates the ScopeModel, and requests a screen redraw every 100 cycles.
 *
 * @param ctx Pointer to the application context.
 */
void adc_timer_cb(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    ScopeModel* model = view_get_model(appScope->view);

    redraw_cnt++;
    model->raw = furi_hal_adc_read(appScope->adc, FuriHalAdcChannel4);
    current_int = !furi_hal_gpio_read(L401DIGILAB_BOARD_PIN_INT);

    if(current_int) {
        model->voltage =
            (double)((double)furi_hal_adc_convert_to_voltage(appScope->adc, model->raw) *
                     (double)(model->BridgeFactor));
    } else {
        model->voltage = (double)0.0;
    }

    if(previous_int != current_int) {
        model->inversion = true;
        model->inversion_count++;
        previous_int = current_int;
    }

    OscWindow_add(model->oscWindow, (uint32_t)(model->voltage * OSC_SCALE_MV));

    if(!(redraw_cnt % 100)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ScopeEventRedraw);
    }
}
/**
 * @brief Draw the oscilloscope waveform and UI elements on the provided canvas.
 *
 * @param canvas Pointer to the canvas used for drawing.
 * @param model  Pointer to the ScopeModel containing waveform data.
 */
void osc_draw_screen(Canvas* canvas, ScopeModel* model) {
    OscWindow_draw(canvas, model->oscWindow);
    if(model->inversion_count > 0) {
        canvas_draw_str_aligned(canvas, 0, 32, AlignLeft, AlignCenter, "OSC");
        model->inversion_count = 0;
    }

    canvas_set_font(canvas, FontSecondary);
    elements_button_right(canvas, "Probe");
    if(model->capture) {
        elements_button_center(canvas, "Stop");
    } else {
        elements_button_center(canvas, "Start");
    }
    elements_button_left(canvas, "Config");
}
/**
 * @brief Render callback for the oscilloscope scene.
 *
 * Clears the canvas, sets the font, and invokes osc_draw_screen().
 *
 * @param canvas Pointer to the canvas for rendering.
 * @param ctx    Pointer to the ScopeModel passed as context.
 */

void app_scope_render_callback(Canvas* canvas, void* ctx) {
    // UNUSED(model);
    ScopeModel* model = (ScopeModel*)ctx;
    // char str[64];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    osc_draw_screen(canvas, model);
}
/**
 * @brief Allocate and initialize the AppScope structure and its resources.
 *
 * Enables OTG if needed, allocates view, model, timer, and configures GPIO/vibrator.
 *
 * @param ctx Pointer to the application context.
 * @return Pointer to the newly allocated AppScope.
 */

AppScope* app_scope_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = malloc(sizeof(AppScope));

    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    appScope->view = view_alloc();
    appScope->list_config = variable_item_list_alloc();
    appScope->timer = furi_timer_alloc(adc_timer_cb, FuriTimerTypePeriodic, app);

    view_allocate_model(appScope->view, ViewModelTypeLockFree, sizeof(ScopeModel));
    with_view_model(
        appScope->view,
        ScopeModel * model,
        {
            model->oscWindow = OscWindow_create(256 - 30, 0, 10, 128, 40);
            model->capture = true;
        },
        false);

    view_set_context(appScope->view, app);
    view_set_draw_callback(appScope->view, app_scope_render_callback);
    view_set_input_callback(appScope->view, app_scope_input_callback);
    view_set_custom_callback(appScope->view, app_scene_scope_on_custom_event);

    furi_hal_gpio_init(L401DIGILAB_BOARD_PIN_ADC, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_vibro, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_vibro, false);

    scope_vibro_stop();

    return appScope;
}
/**
 * @brief Start the periodic scope sampling timer at 1 ms intervals.
 *
 * @param timer Pointer to the FuriTimer used for ADC sampling.
 */
static void scope_timer_start(FuriTimer* timer) {
    furi_timer_start(timer, (furi_kernel_get_tick_frequency() / 1000));
}
/**
 * @brief Stop the periodic scope sampling timer.
 *
 * @param timer Pointer to the FuriTimer to stop.
 */
static void scope_timer_stop(FuriTimer* timer) {
    furi_timer_stop(timer);
}
/**
 * @brief Handle input events for the oscilloscope scene.
 *
 * Processes short key events to navigate, start/stop capture, or switch scenes.
 *
 * @param input_event Pointer to the input event data.
 * @param ctx         Pointer to the application context.
 * @return true if the event was handled; false otherwise.
 */

bool app_scope_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    UNUSED(input_event);
    bool handled = false;

    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyLeft:
            scope_sound_stop();
            scene_manager_next_scene(app->scene_manager, AppSceneScopeConfig);
            break;
        case InputKeyRight:

            scene_manager_search_and_switch_to_another_scene(app->scene_manager, AppSceneProbe);
            break;
        case InputKeyOk:

            AppScope* appScope = (AppScope*)app->sceneScope;

            with_view_model(
                appScope->view,
                ScopeModel * model,
                {
                    model->capture = !model->capture;
                    if(model->capture == true)
                        scope_timer_start(appScope->timer);
                    else
                        scope_timer_stop(appScope->timer);
                },
                true);
            break;
        default:
            break;
        }
    }
    return handled;
}
/**
 * @brief Retrieve the view associated with the oscilloscope scene.
 *
 * @param appScope Pointer to the AppScope instance.
 * @return Pointer to the View used for oscilloscope rendering.
 */

View* app_scope_get_view(AppScope* appScope) {
    furi_assert(appScope);
    return appScope->view;
}
/**
 * @brief Handle generic scene manager events for the oscilloscope scene.
 *
 * Currently unused; always returns false.
 *
 * @param ctx   Pointer to the application context.
 * @param event SceneManagerEvent to process.
 * @return true if the event was consumed; false otherwise.
 */
bool app_scene_scope_on_event(void* ctx, SceneManagerEvent event) {
    bool consumed = false;
    UNUSED(ctx);
    UNUSED(event);

    return consumed;
}
/**
 * @brief Handle custom events, such as redraw, for the oscilloscope scene.
 *
 * On redraw event, updates waveform statistics, triggers feedback, and commits model.
 *
 * @param event Event identifier.
 * @param ctx   Pointer to the application context.
 * @return true if the event was consumed; false otherwise.
 */
bool app_scene_scope_on_custom_event(uint32_t event, void* ctx) {
    bool consumed = false;
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    ScopeModel* model = view_get_model(appScope->view);

    switch(event) {
    case ScopeEventRedraw:
        RingBuffer_getMinMax(
            model->oscWindow->samples, &model->oscWindow->vMin, &model->oscWindow->vMax);
        model->oscWindow->vAvg = RingBuffer_getAverage(model->oscWindow->samples) / OSC_SCALE_V;
        bool alert = CheckAlert(model, app);
        adc_scopeSound(app, model, alert);
        adc_scopeVibro(app, model, alert);
        adc_scopeLed(app, model, alert);
        view_commit_model(appScope->view, true);
        break;
    default:
        break;
    }
    return consumed;
}

/**
 * @brief Callback when the app osc scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_scope_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    ScopeModel* model = view_get_model(appScope->view);

    scope_adc_start(&appScope->adc);
    SK6805_init();
    scope_timer_start(appScope->timer);
    furi_hal_gpio_init(L401DIGILAB_BOARD_PIN_INT, GpioModeInput, GpioPullUp, GpioSpeedLow);

    //Reload latest Bridge Factor
    model->BridgeFactor = (app->data->config->BridgeFactor / 100);
    model->inversion = false;
    model->inversion_count = 0;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewScope);
}

/**
 * @brief Callback invoked when exiting the oscilloscope scene.
 *
 * Stops sampling timer, delays for hardware settle, and shuts down all outputs.
 *
 * @param ctx Pointer to the application context.
 */
void app_scene_scope_on_exit(void* ctx) {
    furi_check(ctx);
    AppContext* app = (AppContext*)ctx;
    AppScope* appScope = (AppScope*)app->sceneScope;
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    scope_timer_stop(appScope->timer);
    furi_delay_ms(100);
    scope_adc_stop(&appScope->adc);
    scope_sound_stop();
    scope_vibro_stop();
    scope_LED_stop();
}

/**
 * @brief Free all resources allocated for the oscilloscope scene.
 *
 * Releases view, model, timer, and memory associated with AppScope.
 *
 * @param appScope Pointer to the AppScope to be freed.
 */
void app_scope_free(AppScope* appScope) {
    furi_assert(appScope);
    variable_item_list_free(appScope->list_config);
    with_view_model(
        appScope->view, ScopeModel * model, { OscWindow_free(model->oscWindow); }, false);
    furi_timer_free(appScope->timer);
    view_free_model(appScope->view);
    view_free(appScope->view);
    free(appScope);
}
