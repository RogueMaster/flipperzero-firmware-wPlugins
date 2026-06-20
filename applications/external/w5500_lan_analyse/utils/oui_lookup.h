#pragma once

#include <stdint.h>

/**
 * Look up a vendor name from the first 3 bytes of a MAC address (OUI).
 * mac: pointer to at least 3 bytes
 * Returns vendor name string, or "Unknown" if not found.
 */
const char* oui_lookup(const uint8_t mac[3]);
