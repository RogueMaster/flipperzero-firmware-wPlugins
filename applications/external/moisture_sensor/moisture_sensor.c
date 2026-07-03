#include "moisture_sensor.h"

// Scene handlers are defined in scenes/scenes.c
extern const SceneManagerHandlers moisture_sensor_scene_handlers;

uint8_t calculate_moisture_percent(uint16_t cal_dry, uint16_t cal_wet, uint16_t raw) {
    if(cal_dry <= cal_wet) return 0;
    if(raw >= cal_dry) return 0;
    if(raw <= cal_wet) return 100;

    uint32_t range = cal_dry - cal_wet;
    uint32_t value = cal_dry - raw;
    return (uint8_t)((value * 100) / range);
}

static int32_t sensor_thread_callback(void* context) {
    MoistureSensorApp* app = context;

    while(app->running) {
        uint32_t sum = 0;
        for(uint8_t i = 0; i < ADC_SAMPLES; i++) {
            sum += furi_hal_adc_read(app->adc_handle, app->adc_channel);
            furi_delay_ms(2);
        }
        uint16_t raw = (uint16_t)(sum / ADC_SAMPLES);
        uint16_t mv = furi_hal_adc_convert_to_voltage(app->adc_handle, raw);

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        uint16_t cal_dry = app->cal_dry_value;
        uint16_t cal_wet = app->cal_wet_value;
        furi_mutex_release(app->mutex);

        bool connected = raw >= SENSOR_MIN_THRESHOLD;
        uint8_t percent = connected ? calculate_moisture_percent(cal_dry, cal_wet, raw) : 0;

        sensor_view_update(app->sensor_view, raw, mv, app->adc_channel, percent, connected);

        furi_delay_ms(SENSOR_POLL_INTERVAL_MS);
    }

    return 0;
}

static bool view_dispatcher_navigation_callback(void* context) {
    MoistureSensorApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool view_dispatcher_custom_event_callback(void* context, uint32_t event) {
    MoistureSensorApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

const GpioPinRecord* ms_furi_hal_resources_pin_by_number(uint8_t number) {
    for(size_t i = 0; i < gpio_pins_count; i++) {
        const GpioPinRecord* record = &gpio_pins[i];
        if(record->number == number) return record;
    }
    return NULL;
}

static MoistureSensorApp* moisture_sensor_app_alloc(void) {
    MoistureSensorApp* app = malloc(sizeof(MoistureSensorApp));
    if(!app) return NULL;

    // Initialize pointers to NULL for safe cleanup
    app->gui = NULL;
    app->view_dispatcher = NULL;
    app->scene_manager = NULL;
    app->notifications = NULL;
    app->sensor_view = NULL;
    app->variable_item_list = NULL;
    app->popup = NULL;
    app->adc_handle = NULL;
    app->gpio_pin = NULL;
    app->sensor_thread = NULL;
    app->mutex = NULL;
    app->running = false;

    // Initialize values
    app->cal_dry_value = ADC_DRY_DEFAULT;
    app->cal_wet_value = ADC_WET_DEFAULT;
    app->edit_dry_value = ADC_DRY_DEFAULT;
    app->edit_wet_value = ADC_WET_DEFAULT;
    app->item_dry = NULL;
    app->item_wet = NULL;
    app->popup_message = NULL;
    app->popup_return_to_main = false;

    // Load calibration
    calibration_load(app);

    // Allocate mutex
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->mutex) goto error;

    // Setup GPIO and ADC
    const GpioPinRecord* pin_record = ms_furi_hal_resources_pin_by_number(SENSOR_PIN_NUMBER);
    if(!pin_record || !pin_record->pin) goto error;

    app->gpio_pin = pin_record->pin;
    app->adc_channel = pin_record->channel;

    app->adc_handle = furi_hal_adc_acquire();
    if(!app->adc_handle) goto error;

    furi_hal_adc_configure_ex(
        app->adc_handle,
        FuriHalAdcScale2500,
        FuriHalAdcClockSync64,
        FuriHalAdcOversample64,
        FuriHalAdcSamplingtime247_5);

    furi_hal_gpio_init(app->gpio_pin, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);

    // Allocate scene manager
    app->scene_manager = scene_manager_alloc(&moisture_sensor_scene_handlers, app);
    if(!app->scene_manager) goto error;

    // Allocate view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) goto error;

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, view_dispatcher_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, view_dispatcher_custom_event_callback);

    // Allocate views
    app->sensor_view = sensor_view_alloc();
    if(!app->sensor_view) goto error;
    view_dispatcher_add_view(
        app->view_dispatcher, MoistureSensorViewSensor, sensor_view_get_view(app->sensor_view));

    app->variable_item_list = variable_item_list_alloc();
    if(!app->variable_item_list) goto error;
    view_dispatcher_add_view(
        app->view_dispatcher,
        MoistureSensorViewMenu,
        variable_item_list_get_view(app->variable_item_list));

    app->popup = popup_alloc();
    if(!app->popup) goto error;
    view_dispatcher_add_view(
        app->view_dispatcher, MoistureSensorViewPopup, popup_get_view(app->popup));

    // Open GUI and notifications
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) goto error;
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Start sensor thread
    app->running = true;
    app->sensor_thread = furi_thread_alloc();
    if(!app->sensor_thread) goto error;

    furi_thread_set_name(app->sensor_thread, "MoistureSensor");
    furi_thread_set_stack_size(app->sensor_thread, 1024);
    furi_thread_set_context(app->sensor_thread, app);
    furi_thread_set_callback(app->sensor_thread, sensor_thread_callback);
    furi_thread_start(app->sensor_thread);

    return app;

error:
    if(app->sensor_thread) {
        app->running = false;
        furi_thread_join(app->sensor_thread);
        furi_thread_free(app->sensor_thread);
    }
    if(app->notifications) furi_record_close(RECORD_NOTIFICATION);
    if(app->gui) furi_record_close(RECORD_GUI);
    if(app->popup) {
        view_dispatcher_remove_view(app->view_dispatcher, MoistureSensorViewPopup);
        popup_free(app->popup);
    }
    if(app->variable_item_list) {
        view_dispatcher_remove_view(app->view_dispatcher, MoistureSensorViewMenu);
        variable_item_list_free(app->variable_item_list);
    }
    if(app->sensor_view) {
        view_dispatcher_remove_view(app->view_dispatcher, MoistureSensorViewSensor);
        sensor_view_free(app->sensor_view);
    }
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    if(app->scene_manager) scene_manager_free(app->scene_manager);
    if(app->adc_handle) furi_hal_adc_release(app->adc_handle);
    if(app->mutex) furi_mutex_free(app->mutex);
    free(app);
    return NULL;
}

static void moisture_sensor_app_free(MoistureSensorApp* app) {
    // Stop sensor thread
    app->running = false;
    furi_thread_join(app->sensor_thread);
    furi_thread_free(app->sensor_thread);

    // Close records
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    // Remove and free views
    view_dispatcher_remove_view(app->view_dispatcher, MoistureSensorViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, MoistureSensorViewMenu);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, MoistureSensorViewSensor);
    sensor_view_free(app->sensor_view);

    // Free dispatcher and scene manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Release hardware
    furi_hal_adc_release(app->adc_handle);

    // Free mutex
    furi_mutex_free(app->mutex);

    free(app);
}

int32_t moisture_sensor_app(void* p) {
    UNUSED(p);

    MoistureSensorApp* app = moisture_sensor_app_alloc();
    if(!app) return -1;

    scene_manager_next_scene(app->scene_manager, MoistureSensorSceneMain);
    view_dispatcher_run(app->view_dispatcher);

    moisture_sensor_app_free(app);
    return 0;
}
