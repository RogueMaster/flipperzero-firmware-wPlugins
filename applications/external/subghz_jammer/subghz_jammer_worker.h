#pragma once

#include "subghz_jammer.h"
#include <furi.h>

typedef struct JammerWorker JammerWorker;

typedef void (*JammerWorkerCallback)(void* context);

JammerWorker* jammer_worker_alloc(JammerState* state, FuriMutex* mutex);
void jammer_worker_free(JammerWorker* worker);
void jammer_worker_set_callback(JammerWorker* worker, JammerWorkerCallback cb, void* ctx);
void jammer_worker_start(JammerWorker* worker);
void jammer_worker_stop(JammerWorker* worker);
bool jammer_worker_is_running(JammerWorker* worker);
