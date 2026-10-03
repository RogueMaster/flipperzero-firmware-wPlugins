#pragma once

#include <furi_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Local copy of lib/subghz's SubGhzWorker, kept only to size the sample stream
 * ourselves: the firmware allocates 4096 LevelDurations (16K) because the same
 * worker also backs RAW recording, where the drain thread waits on the SD card.
 * This app never records RAW, it only decodes, and the firmware's own
 * `subghz rx` CLI command decodes on 1024 samples. 2048 keeps a wide margin for
 * the extra contention here (hopping, BinRAW, the GPS thread, an RPC session)
 * at half the memory.
 *
 * Same API as the firmware worker, minus subghz_worker_set_filter() which this
 * app never called. The thread stack stays at the firmware's 2048: the drain
 * thread runs every protocol decoder in the registry, so it is not worth
 * trimming.
 */
#define SUBGHZ_WARDRIVING_WORKER_BUF_SAMPLES 2048
#define SUBGHZ_WARDRIVING_WORKER_STACK_SIZE  2048

typedef struct SubGhzWarDrivingWorker SubGhzWarDrivingWorker;

typedef void (*SubGhzWarDrivingWorkerOverrunCallback)(void* context);

typedef void (*SubGhzWarDrivingWorkerPairCallback)(void* context, bool level, uint32_t duration);

/**
 * Radio Rx callback, hand this to subghz_devices_start_async_rx().
 */
void subghz_wardriving_worker_rx_callback(bool level, uint32_t duration, void* context);

SubGhzWarDrivingWorker* subghz_wardriving_worker_alloc(void);

void subghz_wardriving_worker_free(SubGhzWarDrivingWorker* instance);

void subghz_wardriving_worker_set_overrun_callback(
    SubGhzWarDrivingWorker* instance,
    SubGhzWarDrivingWorkerOverrunCallback callback);

void subghz_wardriving_worker_set_pair_callback(
    SubGhzWarDrivingWorker* instance,
    SubGhzWarDrivingWorkerPairCallback callback);

void subghz_wardriving_worker_set_context(SubGhzWarDrivingWorker* instance, void* context);

void subghz_wardriving_worker_start(SubGhzWarDrivingWorker* instance);

void subghz_wardriving_worker_stop(SubGhzWarDrivingWorker* instance);

bool subghz_wardriving_worker_is_running(SubGhzWarDrivingWorker* instance);

#ifdef __cplusplus
}
#endif
