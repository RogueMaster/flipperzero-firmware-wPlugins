/*
 * A long-lived connection to the node.
 *
 * The configurator only ever asked questions and waited for answers, so a
 * short-lived worker per scene was enough. The messenger cannot work that way:
 * the node pushes MSG_WAITING whenever a message arrives, and nobody would be
 * listening between requests. So one worker owns the link for as long as the
 * app is connected, pumps it continuously, and splits what arrives into
 * replies (handed to whoever is waiting) and unsolicited events (handed to the
 * application). The split itself lives in meshcore_route.c.
 *
 * Threading contract:
 *   - meshcore_session_request/stream/send may be called from any thread
 *     except the session worker, and they block. Call them from a scene
 *     worker, never from the GUI thread.
 *   - the event callback and the stream collector run ON the worker thread.
 *     They must not block and must not touch a view; post a custom event to
 *     the ViewDispatcher instead.
 */
#pragma once

#include <furi.h>

#include "meshcore_link.h"
#include "meshcore_route.h"

/** Unsolicited event, delivered on the worker thread.
 *
 * `payload` is the raw companion payload behind the event, valid only for the
 * duration of the call. It is what makes codes meshcore_c does not decode
 * usable — RX_LOG_DATA and the telemetry pushes arrive with `event->code` set
 * and everything else only in `payload`. */
typedef void (*MeshCoreSessionEventCallback)(
    const mc_event_t* event,
    const uint8_t* payload,
    size_t len,
    void* context);

/** One frame of a multi-frame reply, delivered on the worker thread.
 *  Return true if it belonged to the stream, false to let it fall through to
 *  the event callback. */
typedef bool (*MeshCoreSessionStreamCallback)(const mc_event_t* event, void* context);

typedef struct MeshCoreSession MeshCoreSession;

/** @param serial_id which hardware port this session drives; a second session
 *                   on the other port is how the Logger runs two nodes. */
MeshCoreSession* meshcore_session_alloc(MeshCoreLog* log, FuriHalSerialId serial_id);
void meshcore_session_free(MeshCoreSession* session);

/** Open the UART and start pumping. False if the USART is unavailable. */
bool meshcore_session_start(MeshCoreSession* session);

/** Stop pumping and release the UART. Returns once the worker has joined. */
void meshcore_session_stop(MeshCoreSession* session);

bool meshcore_session_is_running(MeshCoreSession* session);

/** Register the sink for unsolicited events. Set before starting. */
void meshcore_session_set_event_callback(
    MeshCoreSession* session,
    MeshCoreSessionEventCallback callback,
    void* context);

/** Send a command and wait for `want_code`.
 *
 * Returns true only on `want_code`. On a node error the result is false with
 * `event->code == MC_RESP_ERR`; on silence, `MESHCORE_LINK_NO_EVENT`. */
bool meshcore_session_request(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    uint8_t want_code,
    mc_event_t* event,
    uint32_t timeout_ms);

/** Same, for a command whose reply may be one of several codes.
 *
 * SYNC_NEXT_MESSAGE is the reason this exists: the node answers it with a
 * contact message, a channel message, either V3 variant, or "no more
 * messages", and the caller cannot know which in advance. */
bool meshcore_session_request_any(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    const MeshCoreCodeSet* want,
    mc_event_t* event,
    uint32_t timeout_ms);

/** Same, for a reply that arrives as several frames — every frame before the
 * terminating `want_code` is offered to `collector`. */
bool meshcore_session_request_stream(
    MeshCoreSession* session,
    const uint8_t* payload,
    size_t len,
    uint8_t want_code,
    MeshCoreSessionStreamCallback collector,
    void* collector_context,
    mc_event_t* event,
    uint32_t timeout_ms);

uint32_t meshcore_session_rx_errors(MeshCoreSession* session);
