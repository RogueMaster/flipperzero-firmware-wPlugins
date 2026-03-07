#pragma once

/**
 * rayhunter_worker.h — Poll worker for Ray Hunter Client.
 *
 * The worker owns the UART RX callback.  On each line received from the
 * ESP32 it parses threat level keywords and alert messages, updates the
 * main view model, and posts custom events to the ViewDispatcher for
 * notification and redraw.
 */

#include "rayhunter.h"

/** Allocate and start the worker.  Takes ownership of `uart`. */
RhApp* rh_worker_start(RhApp* app);

/** Stop the worker and release resources.  Does NOT free `app`. */
void rh_worker_stop(RhApp* app);

/**
 * Trigger a poll immediately (called from the FuriTimer callback).
 * Sends "rayhunter_poll\n" to the ESP32 via UART.
 */
void rh_worker_poll(RhApp* app);
