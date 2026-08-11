/*
 * Protocol layer: binds the vendored meshcore_c core to the UART layer.
 *
 * meshcore_c does no I/O — it assembles frames from bytes you feed it and
 * builds command payloads into buffers you own. This file is the only place
 * that connects those two halves to furi_hal_serial.
 *
 * Every call here blocks. Call from a worker thread, never from the GUI
 * thread. See scenes/meshcore_scene_connect.c for the pattern.
 */
#pragma once

#include <furi.h>

#include "meshcore_c/meshcore_companion.h"

#include "../meshcore_log.h"
#include "../uart/meshcore_uart.h"

/* Protocol version this app negotiates: 3 = accept V3 frames (SNR + RSSI),
 * matching what mc_cmd_app_start() advertises. */
#define MESHCORE_LINK_PROTO_VER 3u

/* Name this app reports to the node in APP_START. */
#define MESHCORE_LINK_APP_NAME "MeshCoreCfg"

/* A node that is alive answers well inside this; a node that is not there,
 * or wired TX-to-TX, never will. Kept short so that backing out of a scene
 * does not wait long for the worker to notice. */
#define MESHCORE_LINK_TIMEOUT_MS 1200u

/* ev->code value meaning "no event was decoded" — not a valid wire code. */
#define MESHCORE_LINK_NO_EVENT 0xFFu

typedef struct {
    MeshCoreUart* uart; /* NULL when closed */
    MeshCoreLog* log;
    mc_rx_t rx;
    uint8_t payload[MC_MAX_PAYLOAD]; /* scratch: last assembled payload */
    size_t payload_len;
    bool payload_parsed; /* meshcore_c recognised the code */
    uint8_t frame[MC_RX_BUFSZ]; /* scratch: outbound framed bytes  */
} MeshCoreLink;

/** Zero the struct. Safe to call on an already-closed link. */
void meshcore_link_init(MeshCoreLink* link);

bool meshcore_link_open(MeshCoreLink* link, MeshCoreLog* log, FuriHalSerialId serial_id);
void meshcore_link_close(MeshCoreLink* link);

/** Frame `payload` and push it out. False if it would not fit. */
bool meshcore_link_send(MeshCoreLink* link, const uint8_t* payload, size_t len);

/** Pump the UART until one frame arrives or the timeout expires.
 *
 * Frames whose code meshcore_c does not decode are still reported, with
 * `ev->code` set from the payload and the body reachable through
 * meshcore_link_payload(). That matters: RX_LOG_DATA (0x88) and the telemetry
 * pushes are exactly such codes, and the Logger decodes them itself. Dropping
 * them here would make the whole field-logging mode impossible. */
bool meshcore_link_poll(MeshCoreLink* link, mc_event_t* ev, uint32_t timeout_ms);

/** The payload behind the frame the last poll returned. Valid until the next
 *  poll on this link, so consume it synchronously. */
const uint8_t* meshcore_link_payload(const MeshCoreLink* link, size_t* len);

/** Send `payload`, then wait for a reply carrying `want_code`.
 *
 * Unsolicited pushes (0x80+) that arrive meanwhile are logged and skipped.
 * On a node-side failure the call returns false with `ev->code` set to
 * MC_RESP_ERR; on timeout `ev->code` is MESHCORE_LINK_NO_EVENT.
 *
 * Test-only: the app drives the link through the session worker
 * (meshcore_session_exchange), never this synchronous helper. It exists so the
 * host tests can exercise the send-then-wait logic without a session thread.
 */
bool meshcore_link_request(
    MeshCoreLink* link,
    const uint8_t* payload,
    size_t len,
    uint8_t want_code,
    mc_event_t* ev,
    uint32_t timeout_ms);

/** Line errors seen since open — non-zero points at baud or wiring problems. */
uint32_t meshcore_link_rx_errors(const MeshCoreLink* link);
