// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#pragma once

/**
 * GPS reader: parses NMEA from an external UART GPS module on its own serial
 * port (LPUART by default, so it can run alongside the ESP32 on USART) and
 * publishes the latest fix into the owning ReconApp under its mutex.
 */

#include <stdbool.h>

typedef struct GpsLink GpsLink;

/** @param app  ReconApp* (passed as void* to avoid a header cycle). */
GpsLink* gps_link_alloc(void* app);
void gps_link_free(GpsLink* gps);

/** Acquire the configured serial port and start the worker. */
void gps_link_start(GpsLink* gps);
/** Stop the worker and release the serial port. */
void gps_link_stop(GpsLink* gps);

/**
 * True when the last gps_link_start() could not acquire the configured port --
 * almost always because GPS Port is set to the same UART as the ESP.
 *
 * This used to fail silently: the acquire returned NULL, the worker was torn
 * down, and the UI just kept showing an unlit "searching" badge forever, with no
 * way to tell a cold start from an impossible configuration. Reported on issue
 * #5 by someone who set GPS Port to the ESP's own pins and got no feedback at
 * all. Mirrors EspLinkPortBusy, which exists for exactly this reason on the
 * other link.
 */
bool gps_link_port_busy(GpsLink* gps);

/**
 * Decode one NMEA sentence and publish the fix into the app, under its mutex.
 *
 * Shared by BOTH NMEA sources -- the GPS wired to the Flipper's own UART, and the
 * one relayed by the companion as `G,<nmea>` for boards that wire their GPS to
 * the ESP32 instead (issue #5). Deliberately one function: the lock-loss rule
 * (an explicit RMC 'V' / GGA fixq 0 clears the fix; a garbled-but-"valid"
 * sentence is ignored and keeps the last one) is subtle enough that two copies
 * would drift, and a stale fix silently geotags detections with the wrong place.
 *
 * @param app   ReconApp* (void* to avoid a header cycle).
 * @param line  a single sentence starting at '$'. Tokenized IN PLACE, so it must
 *              be mutable and is not usable afterwards.
 */
void gps_apply_nmea(void* app, char* line);

/**
 * Publish an already-decoded fix into the app, under its mutex.
 *
 * The NMEA-free half of gps_apply_nmea(), exported for the RPC (phone) source,
 * which receives a decoded location and so has no sentence to parse. It exists
 * rather than a second publisher in gps_rpc.c for the reason above: the lock-loss
 * rule is the subtle part, it is identical for every source, and two copies of it
 * would drift into a stale fix geotagging detections with the wrong place.
 *
 * @param app    ReconApp* (void* to avoid a header cycle).
 * @param lat    decimal degrees; ignored unless `valid`.
 * @param lon    decimal degrees; ignored unless `valid`.
 * @param sats   satellite count, or negative to leave the last count alone.
 * @param valid  false explicitly CLEARS the fix (lock lost). True publishes only
 *               if the coordinates also pass gps_coord_sane().
 */
void gps_publish_fix(void* app, float lat, float lon, int sats, bool valid);
