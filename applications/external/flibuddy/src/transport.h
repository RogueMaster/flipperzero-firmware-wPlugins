/* A line-oriented link to the host (NDJSON over USB CDC or BLE).
 *
 * The transport frames received bytes into lines (see line_buffer.h) and
 * hands each one to the rx callback on its own worker thread, so the
 * callback must be thread-safe and quick. send_line() appends the '\n' and
 * may block briefly; it reports whether the host can have the line: not
 * when the port is not open or the host does not drain it, maybe when it
 * stopped draining only at the very end or did not confirm the end.
 * is_connected() reports whether a host holds the port open (DTR for USB),
 * not whether it said `hello` (see host_link.h).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* The backends the app can run; settings files of versions 1 and 2 stored
 * the value (app_settings.h), so the numbers never change. */
typedef enum {
    TransportKindUsb = 0,
    TransportKindBluetooth = 1,
} TransportKind;

#define TRANSPORT_KINDS 2 /* also an index: one slot per kind */

typedef enum {
    TransportSent,
    TransportNotSent, /* the host will not get the line */
    TransportMaybeSent, /* the host may get the line (late) or not */
} TransportSendResult;

typedef void (*TransportRxCallback)(const char* line, size_t len, void* ctx);

typedef struct Transport Transport;

struct Transport {
    bool (*open)(Transport* transport);
    void (*close)(Transport* transport);
    TransportSendResult (*send_line)(Transport* transport, const char* line, size_t len);
    /* Set before open(); the callback must stay valid until close(). */
    void (*set_rx_callback)(Transport* transport, TransportRxCallback callback, void* ctx);
    bool (*is_connected)(const Transport* transport);
};

static inline bool transport_open(Transport* transport) {
    return transport->open(transport);
}

static inline void transport_close(Transport* transport) {
    transport->close(transport);
}

static inline TransportSendResult
    transport_send_line(Transport* transport, const char* line, size_t len) {
    return transport->send_line(transport, line, len);
}

static inline void
    transport_set_rx_callback(Transport* transport, TransportRxCallback callback, void* ctx) {
    transport->set_rx_callback(transport, callback, ctx);
}

static inline bool transport_is_connected(const Transport* transport) {
    return transport->is_connected(transport);
}
