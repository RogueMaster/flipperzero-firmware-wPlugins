// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

/**
 * GPS reader: parses NMEA from an external UART GPS module on its own serial
 * port (LPUART by default, so it can run alongside the ESP32 on USART) and
 * publishes the latest fix into the owning ReconApp under its mutex.
 */

typedef struct GpsLink GpsLink;

/** @param app  ReconApp* (passed as void* to avoid a header cycle). */
GpsLink* gps_link_alloc(void* app);
void gps_link_free(GpsLink* gps);

/** Acquire the configured serial port and start the worker. */
void gps_link_start(GpsLink* gps);
/** Stop the worker and release the serial port. */
void gps_link_stop(GpsLink* gps);
