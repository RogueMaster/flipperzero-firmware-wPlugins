#pragma once
#include <stdbool.h>
#include <stdint.h>

// Universal TV sends on a 100 ms timer. Parsing/transmission count toward this period.
#define IRB_SCAN_PERIOD_MS 100U
uint32_t irb_scan_wait_ms(uint32_t elapsed_ms);

typedef enum {
    IrbScanNone,
    IrbScanPause,
    IrbScanResume,
    IrbScanPrevious,
    IrbScanNext,
    IrbScanReplay,
    IrbScanUse,
    IrbScanStop
} IrbScanCommand;

typedef struct {
    uint32_t total;
    uint32_t current;
    uint32_t last_sent;
    bool paused;
    bool finished;
    bool stopped;
    bool selected;
    bool needs_send;
    bool pending_advance;
} IrbScan;

void irb_scan_init(IrbScan* scan, uint32_t total, uint32_t start);
void irb_scan_command(IrbScan* scan, IrbScanCommand command);
void irb_scan_sent(IrbScan* scan);
void irb_scan_advance(IrbScan* scan);
