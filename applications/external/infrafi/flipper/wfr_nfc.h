#pragma once

#include "protocol/wfr_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Parse WiFi credentials from an NFC tag's raw page memory.
 *
 * Supports NDEF Type 2 tags (NTAG213/215/216, Mifare Ultralight)
 * containing a WiFi Simple Configuration (WSC) record.
 *
 * tag_pages:   pointer to contiguous page data (4 bytes per page)
 * pages_read:  number of pages that were read
 * out_creds:   output credential struct
 *
 * Returns true if WiFi credentials were found and parsed.
 */
bool wfr_nfc_parse_wifi_tag(
    const uint8_t* tag_pages,
    uint16_t pages_read,
    WfrWifiCreds* out_creds);
