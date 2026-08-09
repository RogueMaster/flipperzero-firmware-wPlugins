#pragma once
#include "../../app_user.h"

typedef bool (*rx_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);
typedef void (*rx_handler_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct rx_handle rx_handle_t;

/**
 * F0.5g handler contract:
 *
 * - Predicates and handlers run inside the dispatcher's own mutex
 *   (held since F0.5d to make rx_unregister wait for any in-flight
 *   invocation, closing a use-after-free on stack-allocated ctxs).
 *
 * - This means a slow handler blocks rx_register / rx_unregister
 *   for as long as the handler runs. Keep handlers fast — single-
 *   digit milliseconds at most. Use them to capture state into
 *   `ctx` and signal a waiting thread (semaphore / message queue)
 *   that does the heavy work off-mutex.
 *
 * - Handlers MUST NOT call rx_register or rx_unregister; that
 *   would deadlock against the same mutex they're already holding.
 *
 * - Handlers MAY call chip-mutex-protected functions (send_packet,
 *   etc.) — lock order is dispatch mutex (outer) → chip mutex
 *   (inner) and there is no reverse path.
 *
 * - SnifferScene's handler currently does an SD write
 *   (~10–50 ms typical) inside this lock. That delay surfaces as
 *   Back-button latency on scene exit. Acceptable in practice; if
 *   it ever exceeds tolerance, refactor to enqueue frames in the
 *   handler and drain to SD from the scene's main loop.
 */

void rx_dispatch_init(App* app);
void rx_dispatch_deinit(App* app);

rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx);
void rx_unregister(rx_handle_t* handle);

/**
 * Temporarily stop the dispatcher from consuming chip RX. Used by
 * ethernet_thread around DORA so that DHCP packets reach the worker's
 * own receive_packet calls instead of being eaten (and dropped, since
 * no registered handler matches DHCP) by the dispatcher.
 *
 * pause() blocks until the dispatcher has reached its idle sleep
 * (so the caller can safely call receive_packet right after).
 * resume() unblocks the dispatcher.
 *
 * Calls nest as a flat counter — pause-pause-resume-resume is fine.
 *
 * Proper architectural fix is F0.4b (DHCP becomes a registered
 * handler, no more shared chip access from the worker).
 */
void rx_dispatch_pause(void);
void rx_dispatch_resume(void);
