#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <notification/notification_messages.h>

#define MOVING_AVERAGE_SIZE 16
#define PROGRESS_BAR_WIDTH 100
#define PROGRESS_BAR_HEIGHT 12
#define MAX_DELTA 1500
#define BASELINE_PULSE_WIDTH 1500
#define NORMAL_PULSE_WIDTH 2560

typedef enum {
    AlertModeSoundVibro,
    AlertModeSound,
    AlertModeVibro,
    AlertModeNone
} AlertMode;

typedef struct {
    uint16_t moving_average[MOVING_AVERAGE_SIZE];
    uint16_t average_index;
    uint16_t baseline_value;
    uint16_t current_value;
    bool running;
    Gui* gui;
    NotificationApp* notification;
    FuriThread* sensor_thread;
    ViewPort* view_port;
    FuriMutex* state_mutex;
    volatile uint32_t capture_sum;
    volatile uint32_t capture_count;
    uint16_t sensitivity;
    AlertMode alert_mode;
    bool show_help;
} MetalDetectorApp;

static void moving_average_init(MetalDetectorApp* app) {
    for(int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
        app->moving_average[i] = NORMAL_PULSE_WIDTH;
    }
    app->average_index = 0;
    app->current_value = NORMAL_PULSE_WIDTH;
}

static uint16_t moving_average_update(MetalDetectorApp* app, uint16_t value) {
    app->moving_average[app->average_index] = value;
    app->average_index = (app->average_index + 1) % MOVING_AVERAGE_SIZE;
    
    uint32_t sum = 0;
    for (uint16_t i = 0; i < MOVING_AVERAGE_SIZE; i++) {
        sum += app->moving_average[i];
    }
    return (uint16_t)(sum / MOVING_AVERAGE_SIZE);
}

static void trigger_feedback(MetalDetectorApp* app, uint16_t delta) {
    if (delta < app->sensitivity) return;

    // Trigger display backlight via notification
    notification_message(app->notification, &sequence_display_backlight_on);

    // Scale frequency from 100Hz up to 2000Hz for the active range
    uint16_t active_delta = delta - app->sensitivity;
    uint16_t range = MAX_DELTA > app->sensitivity ? MAX_DELTA - app->sensitivity : 1;
    uint16_t freq = 100 + ((active_delta * 1900) / range);
    if(freq > 2000) freq = 2000;

    bool play_sound = (app->alert_mode == AlertModeSoundVibro || app->alert_mode == AlertModeSound);
    bool play_vibro = (app->alert_mode == AlertModeSoundVibro || app->alert_mode == AlertModeVibro);

    if (play_sound && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_start(freq, 0.5f);
    }
    
    if (play_vibro) furi_hal_vibro_on(true);
    furi_hal_light_set(LightRed, 255);
    
    furi_delay_ms(30); 
    
    if (play_sound && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
    }
    
    if (play_vibro) furi_hal_vibro_on(false);
    furi_hal_light_set(LightRed, 0);
}

static void draw_callback(Canvas* canvas, void* context) {
    if (!context) return;
    
    MetalDetectorApp* app = (MetalDetectorApp*)context;
    
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    
    canvas_clear(canvas);
    
    if (app->show_help) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Metal Detector Help");
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "L/R: Adjust Sensitivity");
        canvas_draw_str(canvas, 2, 35, "OK: Cycle Alert Mode");
        canvas_draw_str(canvas, 2, 46, "Up/Down: Close Help");
        canvas_draw_str(canvas, 2, 57, "Detects metal via RFID freq");
        
        furi_mutex_release(app->state_mutex);
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Metal Detector");
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 100, 10, "? [Up]");
    
    const char* mode_str = "";
    switch(app->alert_mode) {
        case AlertModeSoundVibro: mode_str = "[S+V]"; break;
        case AlertModeSound:      mode_str = "[SND]"; break;
        case AlertModeVibro:      mode_str = "[VIB]"; break;
        case AlertModeNone:       mode_str = "[---]"; break;
    }
    
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "Sens: %u  Mode: %s", app->sensitivity, mode_str);
    canvas_draw_str(canvas, 2, 28, buffer);
    
    int16_t delta = 0;
    if (app->current_value < BASELINE_PULSE_WIDTH) {
        delta = BASELINE_PULSE_WIDTH - app->current_value;
    }

    // Fill the bar over the full range (0 to MAX_DELTA)
    int bar_fill = (delta * PROGRESS_BAR_WIDTH) / MAX_DELTA;
    if (bar_fill > PROGRESS_BAR_WIDTH) bar_fill = PROGRESS_BAR_WIDTH;

    canvas_draw_frame(canvas, 5, 40, PROGRESS_BAR_WIDTH + 4, PROGRESS_BAR_HEIGHT);
    if (bar_fill > 0) {
        canvas_draw_box(canvas, 6, 41, bar_fill, PROGRESS_BAR_HEIGHT - 2);
    }
    
    snprintf(buffer, sizeof(buffer), "Metal: %u", delta);
    canvas_draw_str(canvas, 5, 62, buffer);
    
    furi_mutex_release(app->state_mutex);
}

static void input_callback(InputEvent* input_event, void* context) {
    if (!context) return;
    MetalDetectorApp* app = (MetalDetectorApp*)context;

    if (input_event->type == InputTypePress) {
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        switch (input_event->key) {
            case InputKeyBack:
                app->running = false;
                break;
            case InputKeyLeft:
                if (app->sensitivity > 100) app->sensitivity -= 50;
                break;
            case InputKeyRight:
                if (app->sensitivity < 1400) app->sensitivity += 50;
                break;
            case InputKeyOk:
                app->alert_mode = (app->alert_mode + 1) % 4;
                break;
            case InputKeyUp:
            case InputKeyDown:
                app->show_help = !app->show_help;
                break;
            default:
                break;
        }
        furi_mutex_release(app->state_mutex);
    }
}

static void rfid_capture_callback(bool level, uint32_t duration, void* context) {
    MetalDetectorApp* app = (MetalDetectorApp*)context;
    // Do not strictly filter duration. Accept all positive pulses to compute
    // a true raw average.
    if (level) {
        app->capture_sum += duration;
        app->capture_count++;
    }
}

static int32_t sensor_worker_thread(void* context) {
    MetalDetectorApp* app = (MetalDetectorApp*)context;
    if (!app) return -1;
    
    // Acquire speaker on the worker thread where it is used
    bool speaker_acquired = furi_hal_speaker_acquire(1000);
    
    // Start driving coil continuously to create field
    furi_hal_rfid_tim_read_start(125000, 0.5);

    while (app->running) {
        // Reset counters safely while capture is STOPPED
        app->capture_sum = 0;
        app->capture_count = 0;
        
        // Start DMA timer capture
        furi_hal_rfid_tim_read_capture_start(rfid_capture_callback, app);
        
        // Sample for exactly 50ms
        furi_delay_ms(50);
        
        // Stop capture
        furi_hal_rfid_tim_read_capture_stop();
        
        uint32_t sum = app->capture_sum;
        uint32_t count = app->capture_count;
        
        uint16_t reg_val = 0;
        if (count > 50) {
            // Average duration of the positive half-wave in timer ticks.
            // Multiplied by 10 for precision.
            reg_val = (sum * 10) / count;
        } else {
            // If we didn't get enough valid pulses, it means the comparator
            // froze or chattered excessively. This is a strong indication of metal!
            // We force a large drop to 0 to trigger the detector.
            reg_val = 0;
        }
        
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        app->current_value = moving_average_update(app, reg_val);
        
        int16_t delta = 0;
        if (app->current_value < BASELINE_PULSE_WIDTH) {
            delta = BASELINE_PULSE_WIDTH - app->current_value;
        }
        trigger_feedback(app, delta);
        
        furi_mutex_release(app->state_mutex);
        
        furi_delay_ms(50);
    }
    
    furi_hal_rfid_tim_read_stop();
    
    furi_hal_rfid_pins_reset();
    
    if (speaker_acquired) {
        furi_hal_speaker_release();
    }
    
    return 0;
}

int32_t metal_detector_app(void* p) {
    UNUSED(p);
    
    // Allocate app context
    MetalDetectorApp* app = malloc(sizeof(MetalDetectorApp));
    if (!app) return -1;
    
    memset(app, 0, sizeof(MetalDetectorApp));
    app->running = true;
    app->sensitivity = 750;
    
    // Initialize moving average
    moving_average_init(app);

    // Open GUI and notification services
    app->gui = furi_record_open(RECORD_GUI);
    if (!app->gui) {
        free(app);
        return -1;
    }
    
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    if (!app->notification) {
        furi_record_close(RECORD_GUI);
        free(app);
        return -1;
    }

    // Create mutex for thread-safe state access
    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if (!app->state_mutex) {
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        free(app);
        return -1;
    }

    // Create view port for rendering
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    
    // Add view port to GUI
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Create and start sensor thread
    app->sensor_thread = furi_thread_alloc();
    furi_thread_set_name(app->sensor_thread, "RFID_Sensor");
    furi_thread_set_stack_size(app->sensor_thread, 1024);
    furi_thread_set_callback(app->sensor_thread, sensor_worker_thread);
    furi_thread_set_context(app->sensor_thread, (void*)app);
    furi_thread_start(app->sensor_thread);

    // Main loop - check app->running
    while (app->running) {
        view_port_update(app->view_port);
        furi_delay_ms(50);
    }

    // Signal thread to stop
    app->running = false;
    
    // Wait for sensor thread to finish
    furi_thread_join(app->sensor_thread);
    furi_thread_free(app->sensor_thread);

    // Remove view port from GUI
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    
    // Free mutex
    furi_mutex_free(app->state_mutex);
    
    // Close services
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    
    // Free app context
    free(app);
    
    return 0;
}