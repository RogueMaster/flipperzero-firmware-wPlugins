#pragma once

#include "bambu_rfid.h"

#include <gui/view_dispatcher.h>

struct BrScanResult {
    bool ok;
    char message[64];
    char saved_path[BR_PATH_MAX];
    BrTagInfo info;
};

struct BrScanContext {
    Storage* storage;
    ViewDispatcher* dispatcher;
    volatile bool* cancel;
    uint32_t done_event;
    uint32_t progress_event;
    BrScanResult* result;
};

int32_t br_scan_worker(void* context);
