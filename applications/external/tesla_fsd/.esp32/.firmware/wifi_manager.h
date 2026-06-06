#pragma once

/**
 * wifi_manager.h — WiFi initialisation
 *
 * Tries saved infrastructure WiFi first, then falls back to SoftAP.
 * Call once from setup(); non-fatal on failure.
 */

#include "fsd_handler.h"

/** Start the WiFi AP using credentials from the state. Returns true on success. */
bool wifi_ap_init(const FSDState *state);

/** Try infrastructure WiFi when configured; otherwise start AP. */
bool wifi_init(const FSDState *state);

/** Print the current WiFi dashboard and stream URLs to Serial. */
void wifi_print_status();
