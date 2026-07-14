#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include "ld2410_human_detector_uart.h"

// Define the sensor state model
typedef struct {
    LD2410Data data;
    bool connected;
    bool uart_init_failed;
    uint32_t last_packet_time;
    uint32_t packet_count;
    uint32_t rx_bytes;
} LD2410HumanDetectorModel;

typedef struct {
    LD2410HumanDetectorUart* uart;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    LD2410HumanDetectorModel model;
    FuriMutex* model_mutex;
} LD2410HumanDetectorApp;

typedef enum {
    LD2410HumanDetectorEventInput,
    LD2410HumanDetectorEventDataUpdate,
} LD2410HumanDetectorEventType;

typedef struct {
    LD2410HumanDetectorEventType type;
    InputEvent input;
} LD2410HumanDetectorEvent;

static int32_t ld2410_human_detector_map_distance_to_y(uint16_t distance_cm) {
    const int32_t min_cm = 0;
    const int32_t max_cm = 500;
    const int32_t top_y = 12;
    const int32_t bottom_y = 63;

    int32_t clamped = distance_cm;
    if(clamped < min_cm) clamped = min_cm;
    if(clamped > max_cm) clamped = max_cm;

    int32_t range = bottom_y - top_y;
    return bottom_y - (clamped * range) / max_cm;
}

// UART Callback - runs in worker thread
static void ld2410_human_detector_uart_callback(LD2410Data* data, void* context) {
    LD2410HumanDetectorApp* app = (LD2410HumanDetectorApp*)context;

    furi_mutex_acquire(app->model_mutex, FuriWaitForever);
    app->model.data = *data;
    app->model.connected = true;
    app->model.last_packet_time = furi_get_tick();
    app->model.packet_count++;
    furi_mutex_release(app->model_mutex);

    // Notify main thread to redraw
    // OPTIMIZATION: Do not send event per packet to avoid flooding.
    // Rely on Timer ("Tick") to refresh UI at fixed rate.
    // LD2410HumanDetectorEvent event = {.type = LD2410HumanDetectorEventDataUpdate};
    // furi_message_queue_put(app->event_queue, &event, 0);
}

// Draw Callback
static void ld2410_human_detector_draw_callback(Canvas* canvas, void* context) {
    LD2410HumanDetectorApp* app = (LD2410HumanDetectorApp*)context;

    furi_mutex_acquire(app->model_mutex, FuriWaitForever);
    LD2410HumanDetectorModel model = app->model; // Copy for consistency
    furi_mutex_release(app->model_mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Header
    canvas_draw_str(canvas, 0, 8, "LD2410 Human Detector");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    // Stale check
    bool stale = (furi_get_tick() - model.last_packet_time > 2000);

    if(model.uart_init_failed) {
        canvas_draw_str(canvas, 10, 32, "UART INIT ERROR");
        return;
    }

    if(!model.connected || stale) {
        canvas_draw_str(canvas, 30, 32, stale ? "Data Stale..." : "Waiting data...");
        return;
    }

    // State
    const char* state_str = "Unknown";
    switch(model.data.target_state) {
    case LD2410TargetStateNoTarget:
        state_str = "No Target";
        break;
    case LD2410TargetStateMoving:
        state_str = "Moving";
        break;
    case LD2410TargetStateStatic:
        state_str = "Static";
        break;
    case LD2410TargetStateBoth:
        state_str = "Mov & Stat";
        break;
    }

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "State: %s", state_str);
    canvas_draw_str(canvas, 0, 22, buffer);

    // Distances
    snprintf(buffer, sizeof(buffer), "Det Dist: %d cm", model.data.detection_distance_cm);
    canvas_draw_str(canvas, 0, 32, buffer);

    int32_t dot_y = ld2410_human_detector_map_distance_to_y(model.data.detection_distance_cm);
    const int32_t dot_x = 120;
    for(int32_t dx = 0; dx < 3; dx++) {
        for(int32_t dy = 0; dy < 3; dy++) {
            int32_t y = dot_y + dy - 1;
            if(y >= 12 && y <= 63) {
                canvas_draw_dot(canvas, dot_x + dx, y);
            }
        }
    }

    for(uint16_t cm = 0; cm <= 500; cm += 100) {
        int32_t tick_y = ld2410_human_detector_map_distance_to_y(cm);
        canvas_draw_line(canvas, 119, tick_y, 123, tick_y);
    }

    // Moving Info
    snprintf(
        buffer,
        sizeof(buffer),
        "Mov: %dcm E:%d",
        model.data.move_distance_cm,
        model.data.move_energy);
    canvas_draw_str(canvas, 0, 44, buffer);

    // Static Info
    snprintf(
        buffer,
        sizeof(buffer),
        "Sta: %dcm E:%d",
        model.data.static_distance_cm,
        model.data.static_energy);
    canvas_draw_str(canvas, 0, 56, buffer);
}

// Input Callback
static void ld2410_human_detector_input_callback(InputEvent* input_event, void* context) {
    LD2410HumanDetectorApp* app = (LD2410HumanDetectorApp*)context;
    LD2410HumanDetectorEvent event = {
        .type = LD2410HumanDetectorEventInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, 0);
}

// Timer definition
static void ld2410_human_detector_tick_callback(void* context) {
    LD2410HumanDetectorApp* app = (LD2410HumanDetectorApp*)context;
    // Send empty event just to wake up or specific type
    LD2410HumanDetectorEvent event = {.type = LD2410HumanDetectorEventDataUpdate};
    furi_message_queue_put(app->event_queue, &event, 0);
}

int32_t ld2410_human_detector_app(void* p) {
    UNUSED(p);

    LD2410HumanDetectorApp* app = malloc(sizeof(LD2410HumanDetectorApp));
    memset(app, 0, sizeof(LD2410HumanDetectorApp));

    // Wait for power to stabilize if just turned on
    furi_delay_ms(500);
    app->model.last_packet_time = furi_get_tick();

    app->event_queue = furi_message_queue_alloc(8, sizeof(LD2410HumanDetectorEvent));
    app->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // ViewPort
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, ld2410_human_detector_draw_callback, app);
    view_port_input_callback_set(app->view_port, ld2410_human_detector_input_callback, app);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // UART
    app->uart = ld2410_human_detector_uart_alloc();
    ld2410_human_detector_uart_set_handle_rx_data_cb(
        app->uart, ld2410_human_detector_uart_callback, app);

    bool init_success = ld2410_human_detector_uart_start(app->uart);
    if(!init_success) {
        furi_mutex_acquire(app->model_mutex, FuriWaitForever);
        app->model.uart_init_failed = true;
        furi_mutex_release(app->model_mutex);
    }

    // Timer for debug refresh (20Hz)
    FuriTimer* timer =
        furi_timer_alloc(ld2410_human_detector_tick_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(timer, 50);

    // Main Loop
    LD2410HumanDetectorEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == LD2410HumanDetectorEventInput) {
                if(event.input.key == InputKeyBack && event.input.type == InputTypeShort) {
                    running = false;
                }
            } else if(event.type == LD2410HumanDetectorEventDataUpdate) {
                // Update RX Bytes for debug
                furi_mutex_acquire(app->model_mutex, FuriWaitForever);
                app->model.rx_bytes = ld2410_human_detector_uart_get_rx_bytes(app->uart);
                furi_mutex_release(app->model_mutex);

                view_port_update(app->view_port);
            }
        }
    }

    // Cleanup
    furi_timer_stop(timer);
    furi_timer_free(timer);

    ld2410_human_detector_uart_stop(app->uart);
    ld2410_human_detector_uart_free(app->uart);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_mutex_free(app->model_mutex);
    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}
