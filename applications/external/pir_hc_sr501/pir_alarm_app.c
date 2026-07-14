#include "pir_alarm_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <notification/notification.h>

#define PIR_PIN        &gpio_ext_pc0
#define CHECK_INTERVAL 50
#define ALARM_DURATION 3000
#define BLINK_INTERVAL 150

typedef enum {
    StateIdle,
    StateAlarm,
} AppState;

typedef struct {
    AppState state;
    uint32_t alarm_start;
    uint32_t last_blink;
    bool led_state;
    FuriMessageQueue* event_queue;
} PirAppState;

static void pir_draw_callback(Canvas* const canvas, void* ctx) {
    PirAppState* app_state = ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    if(app_state->state == StateIdle) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 18, "PIR HC-SR501");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Connect:");
        canvas_draw_str(canvas, 2, 38, "VCC->Pin1(5V)");
        canvas_draw_str(canvas, 2, 48, "GND->Pin8(GND)");
        canvas_draw_str(canvas, 2, 58, "SIG->Pin16(C0)");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 40, 28, "MOTION!");
        canvas_draw_str(canvas, 20, 42, "ALARM ACTIVE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 56, "Back to exit");
    }
}

static void pir_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 0);
}

int32_t pir_alarm_app(void* p) {
    UNUSED(p);
    Gui* gui = furi_record_open(RECORD_GUI);
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    furi_hal_gpio_init(PIR_PIN, GpioModeInput, GpioPullNo, GpioSpeedLow);

    PirAppState app_state = {0};
    app_state.state = StateIdle;
    app_state.event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, pir_draw_callback, &app_state);
    view_port_input_callback_set(view_port, pir_input_callback, app_state.event_queue);

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        InputEvent event;
        FuriStatus status = furi_message_queue_get(app_state.event_queue, &event, CHECK_INTERVAL);
        if(status == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }

        if(furi_hal_gpio_read(PIR_PIN) && app_state.state == StateIdle) {
            app_state.state = StateAlarm;
            app_state.alarm_start = furi_get_tick();

            notification_message(notifications, &sequence_double_vibro);
            notification_message(notifications, &sequence_set_only_red_255);
        }

        if(app_state.state == StateAlarm) {
            uint32_t ticks = furi_get_tick();
            if(ticks - app_state.alarm_start > ALARM_DURATION) {
                app_state.state = StateIdle;
                notification_message(notifications, &sequence_reset_red);
                notification_message(notifications, &sequence_success);
            } else {
                if((ticks / BLINK_INTERVAL) % 2) {
                    notification_message(notifications, &sequence_set_only_red_255);
                } else {
                    notification_message(notifications, &sequence_reset_red);
                }
            }
        }

        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(app_state.event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}
