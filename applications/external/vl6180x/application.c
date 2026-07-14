
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include "i2c/vl6180x.h"

#define LOG_TAG "vl6180x_main"

typedef enum {
    EventTypeInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent* input;
} EventApp;

typedef struct {
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    uint8_t address;
    bool stopping;
} VL6180X;

static void draw_callback(Canvas* canvas, void* ctx) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ctx);

    VL6180X* vl6180x = ctx;

    if(vl6180x->stopping) {
        return;
    }

    notification_message(vl6180x->notification, &sequence_display_backlight_on);

    canvas_draw_str(canvas, 0, 10, "VL6180X Distance Sensor");

    if(vl6180x->address == VL6180X_NO_DEVICE_FOUND_ADDRESS) {
        vl6180x->address = find_vl6180x_address();

        // If the sensor is found afterwards, configure it and start it
        if(vl6180x->address != VL6180X_NO_DEVICE_FOUND_ADDRESS) {
            configure_vl6180x(vl6180x->address);
        }
    }

    if(vl6180x->address == VL6180X_NO_DEVICE_FOUND_ADDRESS) {
        canvas_draw_str(canvas, 0, 20, "No VL6180X Found");
    } else {
        uint8_t distance = read_vl6180x_range(vl6180x->address);

        if(distance == VL6180X_FAILED_DISTANCE) {
            canvas_draw_str(canvas, 0, 20, "No VL6180X Found");
            vl6180x->address = VL6180X_NO_DEVICE_FOUND_ADDRESS;
        } else {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Address: %02X", vl6180x->address);
            canvas_draw_str(canvas, 0, 20, buffer);
            snprintf(buffer, sizeof(buffer), "Distance: %d mm", distance);
            canvas_draw_str(canvas, 0, 30, buffer);
        }
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    EventApp event = {.type = EventTypeInput, .input = input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

int32_t vl6180x_app(void* p) {
    FURI_LOG_T(LOG_TAG, __func__);
    UNUSED(p);

    VL6180X vl6180x;
    vl6180x.stopping = false;
    vl6180x.notification = furi_record_open(RECORD_NOTIFICATION);
    vl6180x.address = find_vl6180x_address();

    if(vl6180x.address == VL6180X_NO_DEVICE_FOUND_ADDRESS) {
        FURI_LOG_I(LOG_TAG, "Failed to find VL6180X");
    } else {
        configure_vl6180x(vl6180x.address);
    }

    EventApp event;
    vl6180x.event_queue = furi_message_queue_alloc(8, sizeof(EventApp));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &vl6180x);
    view_port_input_callback_set(view_port, input_callback, vl6180x.event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    view_port_update(view_port);

    while(1) {
        FuriStatus event_status = furi_message_queue_get(vl6180x.event_queue, &event, 100);

        if(event_status == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                if(event.input->key == InputKeyBack) {
                    vl6180x.stopping = true;
                    break;
                }
            }
        }

        view_port_update(view_port);
    }

    furi_message_queue_free(vl6180x.event_queue);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}
