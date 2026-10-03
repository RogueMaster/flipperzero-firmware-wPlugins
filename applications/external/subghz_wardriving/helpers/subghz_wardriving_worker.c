#include "subghz_wardriving_worker.h"

#include <furi.h>

#define TAG "SubGhzWarDrivingWorker"

// Glue durations shorter than this into the neighbouring one, in us. The
// firmware worker exposes this as a setter; nothing here ever changed it.
#define SUBGHZ_WARDRIVING_WORKER_FILTER_US 30

struct SubGhzWarDrivingWorker {
    FuriThread* thread;
    FuriStreamBuffer* stream;

    volatile bool running;
    volatile bool overrun;

    LevelDuration filter_level_duration;
    uint16_t filter_duration;

    SubGhzWarDrivingWorkerOverrunCallback overrun_callback;
    SubGhzWarDrivingWorkerPairCallback pair_callback;
    void* context;
};

void subghz_wardriving_worker_rx_callback(bool level, uint32_t duration, void* context) {
    SubGhzWarDrivingWorker* instance = context;

    LevelDuration level_duration = level_duration_make(level, duration);
    if(instance->overrun) {
        instance->overrun = false;
        level_duration = level_duration_reset();
    }
    size_t ret =
        furi_stream_buffer_send(instance->stream, &level_duration, sizeof(LevelDuration), 0);
    if(sizeof(LevelDuration) != ret) instance->overrun = true;
}

static int32_t subghz_wardriving_worker_thread_callback(void* context) {
    SubGhzWarDrivingWorker* instance = context;

    LevelDuration level_duration;
    while(instance->running) {
        int ret = furi_stream_buffer_receive(
            instance->stream, &level_duration, sizeof(LevelDuration), 10);
        if(ret == sizeof(LevelDuration)) {
            if(level_duration_is_reset(level_duration)) {
                FURI_LOG_E(TAG, "Overrun buffer");
                if(instance->overrun_callback) instance->overrun_callback(instance->context);
            } else {
                bool level = level_duration_get_level(level_duration);
                uint32_t duration = level_duration_get_duration(level_duration);

                if((duration < instance->filter_duration) ||
                   (instance->filter_level_duration.level == level)) {
                    instance->filter_level_duration.duration += duration;

                } else if(instance->filter_level_duration.level != level) {
                    if(instance->pair_callback)
                        instance->pair_callback(
                            instance->context,
                            instance->filter_level_duration.level,
                            instance->filter_level_duration.duration);

                    instance->filter_level_duration.duration = duration;
                    instance->filter_level_duration.level = level;
                }
            }
        }
    }

    return 0;
}

SubGhzWarDrivingWorker* subghz_wardriving_worker_alloc(void) {
    // Zeroed, unlike the firmware worker: running/overrun are read before they
    // would otherwise be written.
    SubGhzWarDrivingWorker* instance = malloc(sizeof(SubGhzWarDrivingWorker));
    memset(instance, 0, sizeof(SubGhzWarDrivingWorker));

    instance->thread = furi_thread_alloc_ex(
        "SubGhzWorker",
        SUBGHZ_WARDRIVING_WORKER_STACK_SIZE,
        subghz_wardriving_worker_thread_callback,
        instance);

    instance->stream = furi_stream_buffer_alloc(
        sizeof(LevelDuration) * SUBGHZ_WARDRIVING_WORKER_BUF_SAMPLES, sizeof(LevelDuration));

    instance->filter_duration = SUBGHZ_WARDRIVING_WORKER_FILTER_US;

    return instance;
}

void subghz_wardriving_worker_free(SubGhzWarDrivingWorker* instance) {
    furi_check(instance);

    furi_stream_buffer_free(instance->stream);
    furi_thread_free(instance->thread);

    free(instance);
}

void subghz_wardriving_worker_set_overrun_callback(
    SubGhzWarDrivingWorker* instance,
    SubGhzWarDrivingWorkerOverrunCallback callback) {
    furi_check(instance);
    instance->overrun_callback = callback;
}

void subghz_wardriving_worker_set_pair_callback(
    SubGhzWarDrivingWorker* instance,
    SubGhzWarDrivingWorkerPairCallback callback) {
    furi_check(instance);
    instance->pair_callback = callback;
}

void subghz_wardriving_worker_set_context(SubGhzWarDrivingWorker* instance, void* context) {
    furi_check(instance);
    instance->context = context;
}

void subghz_wardriving_worker_start(SubGhzWarDrivingWorker* instance) {
    furi_check(instance);
    furi_check(!instance->running);

    instance->running = true;

    furi_thread_start(instance->thread);
}

void subghz_wardriving_worker_stop(SubGhzWarDrivingWorker* instance) {
    furi_check(instance);
    furi_check(instance->running);

    instance->running = false;

    furi_thread_join(instance->thread);
}

bool subghz_wardriving_worker_is_running(SubGhzWarDrivingWorker* instance) {
    furi_check(instance);
    return instance->running;
}
