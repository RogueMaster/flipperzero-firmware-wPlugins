#pragma once

/**
 * wifi_manager.h — WiFi Access Point initialisation
 *
 * Starts a soft-AP with the fixed credentials below.
 * Call once from setup(); non-fatal on failure.
 */

/** Start the WiFi AP.  Returns true on success. */
bool wifi_ap_init();
