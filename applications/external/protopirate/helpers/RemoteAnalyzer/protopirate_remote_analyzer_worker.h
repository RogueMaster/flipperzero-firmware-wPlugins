#pragma once
#include <furi_hal.h>
#include "../../protopirate_app_i.h"

typedef struct ProtoPirateRemoteAnalyzerWorker ProtoPirateRemoteAnalyzerWorker;

typedef void (*ProtoPirateRemoteAnalyzerWorkerPairCallback)(
    void* context,
    uint32_t frequency,
    float rssi,
    bool is_am,
    bool signal);

typedef struct {
    uint32_t frequency_coarse;
    uint32_t frequency_fine;
    float rssi_coarse;
    float rssi_fine;
} FrequencyRSSI;

/** Allocate ProtoPirateRemoteAnalyzerWorker
 * 
 * @param context SubGhz* context
 * @return ProtoPirateRemoteAnalyzerWorker* 
 */
ProtoPirateRemoteAnalyzerWorker* protopirate_remote_analyzer_worker_alloc(void* context);

/** Free ProtoPirateRemoteAnalyzerWorker
 * 
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 */
void protopirate_remote_analyzer_worker_free(ProtoPirateRemoteAnalyzerWorker* instance);

/** Pair callback ProtoPirateRemoteAnalyzerWorker
 * 
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 * @param callback ProtoPirateRemoteAnalyzerWorkerOverrunCallback callback
 * @param context 
 */
void protopirate_remote_analyzer_worker_set_pair_callback(
    ProtoPirateRemoteAnalyzerWorker* instance,
    ProtoPirateRemoteAnalyzerWorkerPairCallback callback,
    void* context);

/** Start ProtoPirateRemoteAnalyzerWorker
 * 
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 * @param txrx pointer to SubGhzTxRx
 */
void protopirate_remote_analyzer_worker_start(ProtoPirateRemoteAnalyzerWorker* instance);

/** Stop ProtoPirateRemoteAnalyzerWorker
 * 
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 */
void protopirate_remote_analyzer_worker_stop(ProtoPirateRemoteAnalyzerWorker* instance);

/** Check if worker is running
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 * @return bool - true if running
 */
bool protopirate_remote_analyzer_worker_is_running(ProtoPirateRemoteAnalyzerWorker* instance);

/** Set RSSI trigger level
 * 
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 * @param value RSSI level
 */
void protopirate_remote_analyzer_worker_set_trigger_level(
    ProtoPirateRemoteAnalyzerWorker* instance,
    float value);

/** Get RSSI trigger level
 * 
 * @param instance ProtoPirateRemoteAnalyzerWorker instance
 * @return RSSI trigger level
 */
float protopirate_remote_analyzer_worker_get_trigger_level(
    ProtoPirateRemoteAnalyzerWorker* instance);

// Round up the frequency
uint32_t protopirate_remote_analyzer_get_nearest_frequency(
    ProtoPirateRemoteAnalyzerWorker* instance,
    uint32_t input);
