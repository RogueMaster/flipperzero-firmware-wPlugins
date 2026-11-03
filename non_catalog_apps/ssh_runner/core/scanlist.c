/* SPDX-License-Identifier: GPL-3.0-only */
#include "scanlist.h"

#include <string.h>

void scan_list_reset(ScanList* l) {
    memset(l, 0, sizeof(*l));
}

void scan_list_merge(ScanList* l, const ProtoScanResult* r) {
    if(r->first == 0) scan_list_reset(l);
    for(uint8_t i = 0; i < r->count && i < PROTO_MAX_SCAN_ENTRIES; i++) {
        unsigned at = (unsigned)r->first + i;
        if(at >= SCAN_LIST_MAX) break;
        l->entries[at] = r->entries[i];
        if(at + 1 > l->count) l->count = (uint8_t)(at + 1);
    }
    if(r->last) l->done = true;
}
