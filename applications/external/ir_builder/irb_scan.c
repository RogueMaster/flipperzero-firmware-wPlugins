#include "irb_scan.h"
#include <string.h>

uint32_t irb_scan_wait_ms(uint32_t elapsed_ms) {
    return elapsed_ms < IRB_SCAN_PERIOD_MS ? IRB_SCAN_PERIOD_MS - elapsed_ms : 0;
}

void irb_scan_init(IrbScan* scan, uint32_t total, uint32_t start) {
    memset(scan, 0, sizeof(*scan));
    scan->total = total;
    scan->current = start < 1 ? 1 : start > total ? total : start;
    scan->stopped = total == 0;
    scan->needs_send = total != 0;
}

void irb_scan_advance(IrbScan* scan) {
    if(scan->stopped || scan->paused || !scan->pending_advance) return;
    scan->pending_advance = false;
    if(scan->current < scan->total) {
        ++scan->current;
        scan->needs_send = true;
    } else {
        scan->paused = scan->finished = true;
    }
}

void irb_scan_sent(IrbScan* scan) {
    scan->last_sent = scan->current;
    scan->needs_send = false;
    scan->pending_advance = true;
}

void irb_scan_command(IrbScan* scan, IrbScanCommand command) {
    if(scan->stopped) return;
    if(command == IrbScanStop) {
        scan->stopped = true;
        scan->needs_send = false;
        return;
    }
    if(command == IrbScanPause) {
        scan->paused = true;
        scan->needs_send = false;
        return;
    }
    if(!scan->paused) return;
    if(command == IrbScanPrevious || command == IrbScanNext) {
        if(command == IrbScanPrevious && scan->current > 1) --scan->current;
        if(command == IrbScanNext && scan->current < scan->total) ++scan->current;
        scan->pending_advance = scan->needs_send = scan->finished = false;
    } else if(command == IrbScanReplay) {
        scan->needs_send = true;
    } else if(command == IrbScanUse) {
        scan->selected = scan->stopped = true;
        scan->needs_send = false;
    } else if(command == IrbScanResume) {
        scan->paused = false;
        if(scan->finished) {
            scan->current = 1;
            scan->finished = scan->pending_advance = false;
            scan->needs_send = true;
        } else if(scan->pending_advance)
            irb_scan_advance(scan);
        else
            scan->needs_send = true;
    }
}
