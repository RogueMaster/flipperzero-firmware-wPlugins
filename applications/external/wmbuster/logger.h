#pragma once
#include <stdint.h>
#include <stddef.h>
#include <storage/storage.h>
#include "protocol/wmbus_link.h"

/* Append a single telegram entry to today's log file at WMBUS_APP_LOG_DIR.
 * Writes one line:  <ms-tick>,<rssi>,<hex-frame>  followed by a JSON line
 * for the parsed link header and a human-readable rendering of the APDU.
 * Returns true on success. */
bool wmbus_log_telegram(
    Storage* storage,
    const uint8_t* frame, size_t len,
    int8_t rssi,
    const WmbusLinkFrame* link,
    const char* parsed_text);
