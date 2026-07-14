#pragma once

#include <furi.h>
#include "protocol/t_union_msg.h"
#include "utils/t_union_msgext.h"

typedef struct TUM_QueryWorker TUM_QueryWorker;

typedef enum {
    TUM_QueryWorkerEventStart,
    TUM_QueryWorkerEventProgress,
    TUM_QueryWorkerEventFinish,
} TUM_QueryWorkerEvent;

typedef void (*TUM_QueryWorkerCallback)(TUM_QueryWorkerEvent event, uint8_t value, void* ctx);

TUM_QueryWorker* tum_query_worker_alloc(TUnionMessage* msg, TUnionMessageExt* msg_ext);
void tum_query_worker_free(TUM_QueryWorker* instance);
void tum_query_worker_start(TUM_QueryWorker* instance);
void tum_query_worker_set_cb(TUM_QueryWorker* instance, TUM_QueryWorkerCallback cb, void* ctx);
