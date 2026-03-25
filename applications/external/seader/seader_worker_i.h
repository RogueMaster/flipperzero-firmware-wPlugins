#pragma once
#define ASN_EMIT_DEBUG 0

#include "seader_i.h"
#include "seader_worker.h"

#include <furi.h>
#include <lib/toolbox/stream/file_stream.h>

#include <furi_hal.h>

#include <stdlib.h>

#include <PAC.h>
#include <SamVersion.h>

#define SEADER_POLLER_MAX_FWT         (200000U)
// Maximum basic rAPDU size is 256 bytes of data + 2 byte SW
#define SEADER_POLLER_MAX_BUFFER_SIZE (258U)

// ATS bit definitions
#define ISO14443_4A_ATS_T0_TA1 (1U << 4)
#define ISO14443_4A_ATS_T0_TB1 (1U << 5)
#define ISO14443_4A_ATS_T0_TC1 (1U << 6)

struct SeaderWorker {
    FuriThread* thread;
    Storage* storage;
    FuriMessageQueue* messages;
    SeaderUartBridge* uart;
    SeaderWorkerCallback callback;
    void* context;

    SeaderPollerEventType stage;
    SeaderWorkerState state;
};

struct SeaderAPDU {
    size_t len;
    uint8_t buf[SEADER_POLLER_MAX_BUFFER_SIZE];
};

void seader_worker_change_state(SeaderWorker* seader_worker, SeaderWorkerState state);

int32_t seader_worker_task(void* context);
