/* Minimal lifecycle example. UI/event-loop setup is intentionally omitted. */
#include "../../sdk/flipper/fib_bridge_client.h"

#include <furi.h>
#include <string.h>

typedef struct {
    FibBridgeClient* bridge;
    bool ready;
    uint8_t preview[256];
    size_t preview_length;
} ExampleApp;

static void example_status(void* context, const FibBridgeStatus* status) {
    ExampleApp* app = context;
    app->ready = status->state == FibBridgeStateReady;
}

static bool example_body(void* context, const uint8_t* data, size_t length) {
    ExampleApp* app = context;
    const size_t available = sizeof(app->preview) - app->preview_length;
    const size_t copied = length < available ? length : available;
    memcpy(app->preview + app->preview_length, data, copied);
    app->preview_length += copied;
    return copied == length;
}

static bool example_bridge_start(ExampleApp* app) {
    const FibBridgeClientConfig config = {.app_version = "1.0"};
    const FibBridgeClientCallbacks callbacks = {
        .on_status = example_status,
        .on_body = example_body,
        .context = app,
    };
    app->bridge = fib_bridge_client_alloc(&config, &callbacks);
    return app->bridge && fib_bridge_client_start(app->bridge);
}

static void example_bridge_poll(ExampleApp* app) {
    fib_bridge_client_tick(app->bridge);
    if(app->ready && !fib_bridge_client_has_active_request(app->bridge)) {
        app->preview_length = 0;
        fib_bridge_client_get(app->bridge, "https://example.com/", 15000);
    }
}

static void example_bridge_stop(ExampleApp* app) {
    if(!app->bridge) return;
    fib_bridge_client_cancel(app->bridge);
    fib_bridge_client_free(app->bridge);
    app->bridge = NULL;
}
