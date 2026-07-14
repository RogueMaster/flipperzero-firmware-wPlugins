#pragma once

#include <furi.h>
#include "spectrum_types.h"

typedef struct SpectrumWorker SpectrumWorker;

typedef void (*SpectrumWorkerCallback)(SpectrumData* data, void* context);

SpectrumWorker* spectrum_worker_alloc(void);
void spectrum_worker_free(SpectrumWorker* worker);
void spectrum_worker_start(
    SpectrumWorker* worker,
    uint32_t freq_start,
    uint32_t freq_end,
    uint32_t step_khz);
void spectrum_worker_stop(SpectrumWorker* worker);
bool spectrum_worker_is_running(SpectrumWorker* worker);
void spectrum_worker_set_callback(
    SpectrumWorker* worker,
    SpectrumWorkerCallback callback,
    void* context);
