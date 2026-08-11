/*
 * Logger mode — the Flipper as a field logger for MeshCore link testing.
 *
 * A node is described by MeshCoreLogNode: which of the two hardware ports it
 * hangs off, what it turned out to be, and the client used to talk to it. Both
 * ports are universal — any node can go on either — so role and hardware are
 * properties discovered per node, not per port. Stage 1 hard-codes companion
 * on the USART; the detection that fills these in properly is stage 4, and the
 * repeater's text CLI is stage 5.
 *
 * Every CSV row is tagged with node/role/hw at the *end* of the line, so the
 * column order meshlog.py produced is untouched and existing pipelines keep
 * working.
 *
 * Position comes from the node, never from the Flipper — the Flipper has no
 * GPS. A node that does not advertise a position logs empty lat/lon/acc, the
 * same as meshlog.py does.
 *
 * 🚨 Do not read a session's CSVs over the Flipper CLI while the logger is
 * running. The files are held open for append, and `storage read` on one of
 * them wedges the storage service — after which every later command that
 * touches storage hangs too, `loader close` included, and the device needs a
 * Back+Left reboot. Stop the logger first (Back, or `loader close`); the files
 * are closed on the way out and every row is already synced to the card.
 */
#pragma once

#include <furi.h>

#include "../meshcore_log.h"
#include "../protocol/meshcore_session.h"

typedef enum {
    MeshCoreLogRoleUnknown,
    MeshCoreLogRoleCompanion, /* binary companion protocol, meshcore_c */
    MeshCoreLogRoleRepeater, /* text CLI; companion protocol not understood */
} MeshCoreLogRole;

typedef enum {
    MeshCoreLogHwUnknown,
    MeshCoreLogHwT114, /* nRF52840 */
    MeshCoreLogHwV4, /* ESP32-S3 */
} MeshCoreLogHw;

typedef struct {
    FuriHalSerialId serial_id;
    MeshCoreSession* session; /* companion client; NULL for a repeater */
    MeshCoreLogRole role;
    MeshCoreLogHw hw;
    char name[24];

    /* Advertised position, degrees x 1e6, straight from the node. */
    int32_t lat;
    int32_t lon;
    bool has_position;
} MeshCoreLogNode;

typedef struct MeshCoreLogger MeshCoreLogger;

MeshCoreLogger* meshcore_logger_alloc(MeshCoreLog* log);
void meshcore_logger_free(MeshCoreLogger* logger);

/** Create the session directory, open the CSVs, attach to the node and start
 *  recording. Blocks (it talks to the node), so call it from a worker thread.
 *  On failure meshcore_logger_error() explains why. */
bool meshcore_logger_start(MeshCoreLogger* logger);

void meshcore_logger_stop(MeshCoreLogger* logger);
bool meshcore_logger_is_running(MeshCoreLogger* logger);

/** NULL while everything is fine. */
const char* meshcore_logger_error(MeshCoreLogger* logger);

/** Directory this session is writing into, for the UI to show. */
const char* meshcore_logger_session_path(MeshCoreLogger* logger);

const MeshCoreLogNode* meshcore_logger_node(MeshCoreLogger* logger);

uint32_t meshcore_logger_rx_count(MeshCoreLogger* logger);
/** Rows lost because the writer could not keep up with the radio. Should stay
 *  at zero; anything else means the SD card is the bottleneck. */
uint32_t meshcore_logger_dropped(MeshCoreLogger* logger);

/** Most recent reception, for the live display. False before the first one. */
bool meshcore_logger_last_rx(MeshCoreLogger* logger, int8_t* snr_q4, int8_t* rssi);

/** Record "I am standing here now" in events.csv.
 *
 *  Safe to call from the GUI thread: it only formats a row and posts it to the
 *  writer, and never touches the card or the link. */
void meshcore_logger_mark(MeshCoreLogger* logger);

/** How many marks this session has, so the display can show the number that
 *  was just written and the operator can say it out loud. */
uint32_t meshcore_logger_marks(MeshCoreLogger* logger);

/** Pings attempted and answered, for the live display. The ratio is the loss
 *  figure the acceptance criteria are stated in. */
void meshcore_logger_ping_stats(MeshCoreLogger* logger, uint32_t* sent, uint32_t* ok);
