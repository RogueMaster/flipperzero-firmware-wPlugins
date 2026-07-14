#include "uart_pong.h"

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    UartPongApp* app = ctx;
    UNUSED(app);

    canvas_clear(canvas);

    canvas_draw_box(canvas, 0, app->me.y, 3, 12);
    canvas_draw_box(canvas, 128 - 3, app->enemy.y, 3, 12);

    canvas_draw_box(canvas, (int32_t)app->ball.x, (int32_t)app->ball.y, 3, 3);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d : %d", app->me.score, app->enemy.score);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, buf);

    float deg = atan2(app->ball.dy, app->ball.dx) * (double)(180.0 / M_PI);
    snprintf(buf, sizeof(buf), "angle: %.2f", (double)deg);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, buf);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void uart_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    furi_assert(ctx);
    UartPongApp* app = ctx;
    UNUSED(app);

    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        app->enemy.y = byte;
    }
}

static void game_tick(void* ctx) {
    furi_assert(ctx);
    UartPongApp* app = ctx;

    uint8_t prev_y = app->me.y;

    if(app->controls.up && app->me.y > 0) app->me.y--;

    if(app->controls.down && app->me.y < 63 - 12) app->me.y++;

    if(prev_y != app->me.y) furi_hal_serial_tx(app->handle, &app->me.y, 1);

    app->ball.y += app->ball.dy;
    app->ball.x += app->ball.dx;

    if(app->ball.y < 1 || app->ball.y > 63 - 3) app->ball.dy *= -1;

    if(app->ball.x < 1 || app->ball.x > 127 - 3) app->ball.dx *= -1;

    if(app->ball.x <= 3 && app->ball.y > app->me.y && app->ball.y < app->me.y + 12) {
        // ToDo:
    }

    view_port_update(app->view_port);
}

static UartPongApp* uart_pong_alloc() {
    UartPongApp* app = malloc(sizeof(UartPongApp));
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gui = furi_record_open(RECORD_GUI);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    app->handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_hal_serial_init(app->handle, 9600);
    furi_hal_serial_async_rx_start(app->handle, uart_callback, app, true);

    app->timer = furi_timer_alloc(game_tick, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(1000 / FPS));
    furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);

    app->ball.x = 32.f;
    app->ball.y = 16.f;

    float angle = degToRad(45.f);
    app->ball.dx = (float)cos(angle) * SPEED;
    app->ball.dy = (float)sin(angle) * SPEED;

    return app;
}

static void uart_pong_free(UartPongApp* app) {
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    furi_hal_serial_async_rx_stop(app->handle);
    furi_hal_serial_deinit(app->handle);
    furi_hal_serial_control_release(app->handle);

    furi_mutex_release(app->mutex);
    furi_mutex_free(app->mutex);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t uart_pong_app(void* p) {
    UNUSED(p);
    UartPongApp* app = uart_pong_alloc();

    InputEvent event;

    while(true) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeShort) {
                furi_mutex_release(app->mutex);
                break;
            };

            if(event.key == InputKeyUp && event.type == InputTypePress) app->controls.up = true;

            if(event.key == InputKeyDown && event.type == InputTypePress)
                app->controls.down = true;

            if(event.key == InputKeyUp && event.type == InputTypeRelease) app->controls.up = false;

            if(event.key == InputKeyDown && event.type == InputTypeRelease)
                app->controls.down = false;
        }
    }

    uart_pong_free(app);

    return 0;
}
