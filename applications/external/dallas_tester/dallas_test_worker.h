#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Snapshot of one measurement batch. Timing/ROM fields are valid only when samples_ok > 0;
// rom[] is valid only when rom_valid.
typedef struct {
    uint8_t samples_ok; // valid presence pulses captured this batch (0..DALLAS_TEST_SAMPLES)
    bool bus_fault; // no valid pulse and the bus was stuck low (shorted/stuck contacts)
    uint16_t tpdl_us; // median presence-detect-low time
    uint16_t tpdl_min_us;
    uint16_t tpdl_max_us;
    uint16_t tpdl_jitter_us; // population stddev of tPDL
    uint16_t tpdh_us; // median master-release -> slave-pull-low time
    bool rom_valid; // ROM read back with a valid Maxim CRC8
    uint8_t rom[8]; // [0]=family code .. [7]=CRC
} DallasTestResult;

// Invoked from the worker thread after each measurement batch. Must be short and
// thread-safe: the test scene marshals the data into its view model under the model lock.
typedef void (*DallasTestResultCallback)(const DallasTestResult* result, void* context);

typedef struct DallasTestWorker DallasTestWorker;

DallasTestWorker* dallas_test_worker_alloc(void);
void dallas_test_worker_free(DallasTestWorker* worker);

// Starts the background measurement loop. callback fires ~every DALLAS_BATCH_INTERVAL_MS.
void dallas_test_worker_start(
    DallasTestWorker* worker,
    DallasTestResultCallback callback,
    void* context);

// Stops and joins the worker thread and releases the 1-Wire pin. Safe to call if not running.
void dallas_test_worker_stop(DallasTestWorker* worker);

#ifdef __cplusplus
}
#endif
