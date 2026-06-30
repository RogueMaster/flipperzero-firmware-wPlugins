#include "radio_scanner_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <gui/elements.h>
#include <furi_hal_speaker.h>
#include <subghz/devices/devices.h>

#define TAG "RadioScannerApp"

#define RADIO_SCANNER_DEFAULT_FREQ        433920000
#define RADIO_SCANNER_DEFAULT_RSSI        (-100.0f)
#define RADIO_SCANNER_DEFAULT_SENSITIVITY (-85.0f)
#define RADIO_SCANNER_BUFFER_SZ           32

#define SUBGHZ_FREQUENCY_MIN  300000000
#define SUBGHZ_FREQUENCY_MAX  928000000
#define SUBGHZ_FREQUENCY_STEP 10000
#define SUBGHZ_DEVICE_NAME    "cc1101_int"

static void radio_scanner_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_draw_callback");
#endif
    RadioScannerApp* app = (RadioScannerApp*)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Radio Scanner");

    canvas_set_font(canvas, FontSecondary);
    char freq_str[RADIO_SCANNER_BUFFER_SZ + 1] = {0};
    snprintf(
        freq_str, RADIO_SCANNER_BUFFER_SZ, "Freq: %.2f MHz", (double)app->frequency / 1000000);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, freq_str);

    char rssi_str[RADIO_SCANNER_BUFFER_SZ + 1] = {0};
    snprintf(rssi_str, RADIO_SCANNER_BUFFER_SZ, "RSSI: %.2f", (double)app->rssi);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, rssi_str);

    char sensitivity_str[RADIO_SCANNER_BUFFER_SZ + 1] = {0};
    snprintf(sensitivity_str, RADIO_SCANNER_BUFFER_SZ, "Sens: %.2f", (double)app->sensitivity);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, sensitivity_str);

    canvas_draw_str_aligned(
        canvas, 64, 54, AlignCenter, AlignTop, app->scanning ? "Scanning..." : "Locked");
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_draw_callback");
#endif
}

static void radio_scanner_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_input_callback");
    FURI_LOG_D(TAG, "Input event: type=%d, key=%d", input_event->type, input_event->key);
#endif
    FuriMessageQueue* event_queue = context;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    FURI_LOG_D(TAG, "Exit radio_scanner_input_callback");
}

static void radio_scanner_rx_callback(const void* data, size_t size, void* context) {
    UNUSED(data);
    UNUSED(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "radio_scanner_rx_callback called with size: %zu", size);
#else
    UNUSED(size);
#endif
}

static void radio_scanner_update_rssi(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_update_rssi");
#endif
    if(app->radio_device) {
        app->rssi = subghz_devices_get_rssi(app->radio_device);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Updated RSSI: %f", (double)app->rssi);
#endif
    } else {
        FURI_LOG_E(TAG, "Radio device is NULL");
        app->rssi = RADIO_SCANNER_DEFAULT_RSSI;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_update_rssi");
#endif
}

static bool radio_scanner_init_subghz(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_init_subghz");
#endif
    subghz_devices_init();
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGHz devices initialized");
#endif

    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_NAME);
    if(!device) {
        FURI_LOG_E(TAG, "Failed to get SubGhzDevice");
        return false;
    }
    FURI_LOG_I(TAG, "SubGhzDevice obtained: %s", subghz_devices_get_name(device));

    app->radio_device = device;

    subghz_devices_begin(device);
    subghz_devices_reset(device);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGhzDevice begun");
#endif
    if(!subghz_devices_is_frequency_valid(device, app->frequency)) {
        FURI_LOG_E(TAG, "Invalid frequency: %lu", app->frequency);
        return false;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Frequency is valid: %lu", app->frequency);
#endif
    subghz_devices_load_preset(device, FuriHalSubGhzPreset2FSKDev238Async, NULL);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Preset loaded");
#endif
    subghz_devices_set_frequency(device, app->frequency);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Frequency set to %lu", app->frequency);
#endif
    subghz_devices_start_async_rx(device, radio_scanner_rx_callback, app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Asynchronous RX started");
#endif
    if(furi_hal_speaker_acquire(30)) {
        app->speaker_acquired = true;
        subghz_devices_set_async_mirror_pin(device, &gpio_speaker);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Speaker acquired and async mirror pin set");
#endif
    } else {
        app->speaker_acquired = false;
        FURI_LOG_E(TAG, "Failed to acquire speaker");
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_init_subghz");
#endif
    return true;
}

static void radio_scanner_process_scanning(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_process_scanning");
#endif
    radio_scanner_update_rssi(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "RSSI after update: %f", (double)app->rssi);
#endif
    bool signal_detected = (app->rssi > app->sensitivity);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Signal detected: %d", signal_detected);
#endif

    if(signal_detected) {
        if(app->scanning) {
            app->scanning = false;
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning stopped");
#endif
        }
    } else {
        if(!app->scanning) {
            app->scanning = true;
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning started");
#endif
        }
    }

    if(!app->scanning) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Exit radio_scanner_process_scanning");
        return;
#endif
    }
    uint32_t new_frequency = (app->scan_direction == ScanDirectionUp) ?
                                 app->frequency + SUBGHZ_FREQUENCY_STEP :
                                 app->frequency - SUBGHZ_FREQUENCY_STEP;

    if(!subghz_devices_is_frequency_valid(app->radio_device, new_frequency)) {
        if(app->scan_direction == ScanDirectionUp) {
            if(new_frequency < 387000000) {
                new_frequency = 387000000;
            } else if(new_frequency < 779000000) {
                new_frequency = 779000000;
            } else if(new_frequency > SUBGHZ_FREQUENCY_MAX) {
                new_frequency = SUBGHZ_FREQUENCY_MIN;
            }
        } else {
            if(new_frequency > 464000000) {
                new_frequency = 464000000;
            } else if(new_frequency > 348000000) {
                new_frequency = 348000000;
            } else if(new_frequency < SUBGHZ_FREQUENCY_MIN) {
                new_frequency = SUBGHZ_FREQUENCY_MAX;
            }
        }
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Adjusted frequency to next valid range: %lu", new_frequency);
#endif
    }

    subghz_devices_flush_rx(app->radio_device);
    subghz_devices_stop_async_rx(app->radio_device);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Asynchronous RX stopped");
#endif

    subghz_devices_idle(app->radio_device);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Device set to idle");
#endif
    app->frequency = new_frequency;
    subghz_devices_set_frequency(app->radio_device, app->frequency);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Frequency set to %lu", app->frequency);
#endif

    subghz_devices_start_async_rx(app->radio_device, radio_scanner_rx_callback, app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Asynchronous RX restarted");
    FURI_LOG_D(TAG, "Exit radio_scanner_process_scanning");
#endif
}

RadioScannerApp* radio_scanner_app_alloc() {
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_app_alloc");
#endif
    RadioScannerApp* app = malloc(sizeof(RadioScannerApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate RadioScannerApp");
        return NULL;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "RadioScannerApp allocated");
#endif

    app->view_port = view_port_alloc();
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort allocated");
#endif

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Event queue allocated");
#endif

    app->running = true;
    app->frequency = RADIO_SCANNER_DEFAULT_FREQ;
    app->rssi = RADIO_SCANNER_DEFAULT_RSSI;
    app->sensitivity = RADIO_SCANNER_DEFAULT_SENSITIVITY;
    app->scanning = true;
    app->scan_direction = ScanDirectionUp;
    app->speaker_acquired = false;
    app->radio_device = NULL;

    view_port_draw_callback_set(app->view_port, radio_scanner_draw_callback, app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Draw callback set");
#endif

    view_port_input_callback_set(app->view_port, radio_scanner_input_callback, app->event_queue);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Input callback set");
    FURI_LOG_D(TAG, "Exit radio_scanner_app_alloc");
#endif

    app->gui = furi_record_open(RECORD_GUI);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "GUI record opened");
#endif

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort added to GUI");
#endif

    return app;
}

void radio_scanner_app_free(RadioScannerApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter radio_scanner_app_free");
#endif
    if(app->speaker_acquired && furi_hal_speaker_is_mine()) {
        subghz_devices_set_async_mirror_pin(app->radio_device, NULL);
        furi_hal_speaker_release();
        app->speaker_acquired = false;
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Speaker released");
#endif
    }

    if(app->radio_device) {
        subghz_devices_flush_rx(app->radio_device);
        subghz_devices_stop_async_rx(app->radio_device);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Asynchronous RX stopped");
#endif
        subghz_devices_idle(app->radio_device);
        subghz_devices_sleep(app->radio_device);
        subghz_devices_end(app->radio_device);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "SubGhzDevice stopped and ended");
#endif
    }

    subghz_devices_deinit();
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGHz devices de-initialized");
#endif
    gui_remove_view_port(app->gui, app->view_port);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort removed from GUI");
#endif
    view_port_free(app->view_port);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "ViewPort freed");
#endif

    furi_message_queue_free(app->event_queue);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Event queue freed");
#endif

    furi_record_close(RECORD_GUI);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "GUI record closed");
#endif

    free(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "RadioScannerApp memory freed");
#endif
}

int32_t radio_scanner_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Enter radio_scanner_app");

    RadioScannerApp* app = radio_scanner_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return 1;
    }

    if(!radio_scanner_init_subghz(app)) {
        FURI_LOG_E(TAG, "Failed to initialize SubGHz");
        radio_scanner_app_free(app);
        return 255;
    }
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "SubGHz initialized successfully");
#endif

    InputEvent event;
    while(app->running) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Main loop iteration");
#endif
        if(app->scanning) {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning is active");
#endif
            radio_scanner_process_scanning(app);
        } else {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Scanning is inactive, updating RSSI");
#endif
            radio_scanner_update_rssi(app);
        }

#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Checking for input events");
#endif
        if(furi_message_queue_get(app->event_queue, &event, 10) == FuriStatusOk) {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Input event received: type=%d, key=%d", event.type, event.key);
#endif
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyOk) {
                    app->scanning = !app->scanning;
                    FURI_LOG_I(TAG, "Toggled scanning: %d", app->scanning);
                } else if(event.key == InputKeyUp) {
                    app->sensitivity += 1.0f;
                    FURI_LOG_I(TAG, "Increased sensitivity: %f", (double)app->sensitivity);
                } else if(event.key == InputKeyDown) {
                    app->sensitivity -= 1.0f;
                    FURI_LOG_I(TAG, "Decreased sensitivity: %f", (double)app->sensitivity);
                } else if(event.key == InputKeyLeft) {
                    app->scan_direction = ScanDirectionDown;
                    FURI_LOG_I(TAG, "Scan direction set to down");
                } else if(event.key == InputKeyRight) {
                    app->scan_direction = ScanDirectionUp;
                    FURI_LOG_I(TAG, "Scan direction set to up");
                } else if(event.key == InputKeyBack) {
                    app->running = false;
                    FURI_LOG_I(TAG, "Exiting app");
                }
            }
        }

        view_port_update(app->view_port);
        furi_delay_ms(10);
    }

    radio_scanner_app_free(app);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Exit radio_scanner_app");
#endif
    return 0;
}
