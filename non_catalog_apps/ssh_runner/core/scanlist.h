/* The WiFi scan as one list: the board sends it in SCAN_RESULT events of up to 8 networks
 * each (PROTOCOL.md), so nine or more networks arrive in two or more. Pure C, host-tested
 * (tests/test_session.c). SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <proto/proto_msg.h>
#include <stdbool.h>

#define SCAN_LIST_MAX 16 /* as many as the WiFi list shows (WIFI_LIST_MAX) */

typedef struct {
    ProtoScanEntry entries[SCAN_LIST_MAX];
    uint8_t count;
    bool done; /* the event marked last has arrived */
} ScanList;

void scan_list_reset(ScanList* l);
/* One event: entries go to their place (`first`); an event at 0 starts a new scan. Networks
 * past SCAN_LIST_MAX are left out. */
void scan_list_merge(ScanList* l, const ProtoScanResult* r);
