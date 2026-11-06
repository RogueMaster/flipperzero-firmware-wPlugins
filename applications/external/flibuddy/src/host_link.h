/* The `hello` handshake with the host, as a pure state machine.
 *
 *   Closed ──port opened──▶ Waiting ──host hello──▶ Up
 *                              │ 2 s                ▲
 *                              ▼                    │
 *                            NoHost ───host hello───┘
 *
 * When the host opens the port (DTR raised) the Flipper sends `hello` once
 * and waits; a host `hello` within HOST_LINK_HELLO_TIMEOUT_MS makes the host
 * connected. Without an answer the port is held by something else (a
 * terminal, qFlipper) and the Flipper stays quiet until a host `hello`
 * arrives, which connects at any time and is answered with a `hello` unless
 * it answers ours. Closing the port loses the host. A host `hello` while
 * the port is closed is stale and ignored: the caller feeds port_opened()
 * first when the transport already holds the port.
 *
 * Each call returns what the caller must do. Times are milliseconds of a
 * free-running uint32 tick that may wrap.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HOST_LINK_HELLO_TIMEOUT_MS 2000

typedef enum {
    HostLinkStateClosed, /* port not open */
    HostLinkStateWaiting, /* our hello sent, waiting for the host's */
    HostLinkStateNoHost, /* port open, nobody answered */
    HostLinkStateUp, /* host connected */
} HostLinkState;

typedef struct {
    bool send_hello;
    bool host_connected; /* feed AppEventHostConnected */
    bool host_lost; /* feed AppEventHostLost */
} HostLinkOutput;

typedef struct {
    HostLinkState state;
    uint32_t hello_sent_at;
} HostLink;

void host_link_init(HostLink* link);

HostLinkOutput host_link_port_opened(HostLink* link, uint32_t now_ms);
HostLinkOutput host_link_port_closed(HostLink* link);
HostLinkOutput host_link_host_hello(HostLink* link, uint32_t now_ms);
HostLinkOutput host_link_tick(HostLink* link, uint32_t now_ms);

bool host_link_is_up(const HostLink* link);
