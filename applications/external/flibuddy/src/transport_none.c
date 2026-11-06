#include "transport_none.h"

static bool none_open(Transport* transport) {
    (void)transport;
    return false;
}

static void none_close(Transport* transport) {
    (void)transport;
}

static TransportSendResult none_send_line(Transport* transport, const char* line, size_t len) {
    (void)transport;
    (void)line;
    (void)len;
    return TransportNotSent;
}

static void none_set_rx_callback(Transport* transport, TransportRxCallback callback, void* ctx) {
    (void)transport;
    (void)callback;
    (void)ctx;
}

static bool none_is_connected(const Transport* transport) {
    (void)transport;
    return false;
}

static Transport none = {
    .open = none_open,
    .close = none_close,
    .send_line = none_send_line,
    .set_rx_callback = none_set_rx_callback,
    .is_connected = none_is_connected,
};

Transport* transport_none(void) {
    return &none;
}
