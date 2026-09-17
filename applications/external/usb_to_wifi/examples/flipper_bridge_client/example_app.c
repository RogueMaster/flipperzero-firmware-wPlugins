#include "../../sdk/flipper/fib_bridge_client.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only public SDK calls; worker and GUI share a bounded, mutex-protected model. */
typedef struct {
    FibBridgeClient* bridge;
    FuriMutex* mutex;
    FuriMessageQueue* input;
    FibBridgeStatus status;
    char preview[257];
    size_t length;
    size_t page;
} ExampleApp;

static bool example_body(void* context, const uint8_t* data, size_t length) {
    ExampleApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; i < length && app->length < sizeof(app->preview) - 1; ++i) {
        uint8_t byte = data[i];
        app->preview[app->length++] = byte >= 32 && byte <= 126 ? (char)byte : ' ';
    }
    app->preview[app->length] = '\0';
    furi_mutex_release(app->mutex);
    return true; /* Drain remaining bytes without accumulating the full body. */
}

static void example_draw(Canvas* canvas, void* context) {
    ExampleApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 9, "Bridge SDK Client");
    char line[32];
    snprintf(
        line,
        sizeof(line),
        "HTTP %u | %lu B",
        app->status.http_status,
        (unsigned long)app->status.response_bytes);
    canvas_draw_str(canvas, 0, 20, line);
    const char* text = app->length ? app->preview : app->status.detail;
    size_t length = strlen(text);
    for(size_t row = 0; row < 3; ++row) {
        size_t offset = app->page * 54 + row * 18;
        size_t count = offset < length ? length - offset : 0;
        if(count > 18) count = 18;
        if(count) memcpy(line, text + offset, count);
        line[count] = '\0';
        canvas_draw_str(canvas, 0, 31 + row * 10, line);
    }
    canvas_draw_str(
        canvas, 0, 63, app->status.active_request ? "BACK: cancel + exit" : "OK: GET BACK: exit");
    furi_mutex_release(app->mutex);
}

static void example_input(InputEvent* event, void* context) {
    ExampleApp* app = context;
    if(event->type == InputTypeShort) furi_message_queue_put(app->input, event, 0);
}

int32_t fib_sdk_example_main(void* argument) {
    UNUSED(argument);
    ExampleApp* app = calloc(1, sizeof(ExampleApp));
    if(!app) return -1;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    if(!app->mutex || !app->input) {
        if(app->input) furi_message_queue_free(app->input);
        if(app->mutex) furi_mutex_free(app->mutex);
        free(app);
        return -1;
    }
    const FibBridgeClientConfig config = {.app_version = "sdk-example-1"};
    const FibBridgeClientCallbacks callbacks = {.on_body = example_body, .context = app};
    app->bridge = fib_bridge_client_alloc(&config, &callbacks);
    if(!app->bridge || !fib_bridge_client_start(app->bridge)) {
        if(app->bridge) fib_bridge_client_free(app->bridge);
        furi_message_queue_free(app->input);
        furi_mutex_free(app->mutex);
        free(app);
        return -1;
    }
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* viewport = view_port_alloc();
    if(!viewport) {
        furi_record_close(RECORD_GUI);
        fib_bridge_client_free(app->bridge);
        furi_message_queue_free(app->input);
        furi_mutex_free(app->mutex);
        free(app);
        return -1;
    }
    view_port_draw_callback_set(viewport, example_draw, app);
    view_port_input_callback_set(viewport, example_input, app);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);
    bool running = true;
    while(running) {
        fib_bridge_client_tick(app->bridge);
        FibBridgeStatus status;
        fib_bridge_client_get_status(app->bridge, &status);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->status = status;
        furi_mutex_release(app->mutex);
        view_port_update(viewport);
        InputEvent event;
        if(furi_message_queue_get(app->input, &event, 100) != FuriStatusOk) continue;
        if(event.key == InputKeyBack) {
            fib_bridge_client_cancel(app->bridge);
            running = false;
        } else if(event.key == InputKeyOk && fib_bridge_client_is_ready(app->bridge)) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->length = 0;
            app->preview[0] = '\0';
            app->page = 0;
            furi_mutex_release(app->mutex);
            fib_bridge_client_get(app->bridge, "https://api.github.com/zen", 15000);
        } else if(event.key == InputKeyLeft || event.key == InputKeyRight) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            size_t length = app->length ? app->length : strlen(app->status.detail);
            if(event.key == InputKeyLeft && app->page > 0) --app->page;
            if(event.key == InputKeyRight && (app->page + 1) * 54 < length) ++app->page;
            furi_mutex_release(app->mutex);
        }
    }
    view_port_enabled_set(viewport, false);
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    /* Join transport callbacks before freeing their context and mutex. */
    fib_bridge_client_free(app->bridge);
    furi_message_queue_free(app->input);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
