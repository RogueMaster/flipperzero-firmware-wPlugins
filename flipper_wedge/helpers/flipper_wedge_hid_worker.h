#pragma once

#include <furi.h>
#include "flipper_wedge_hid.h"

typedef struct FlipperWedgeHidWorker FlipperWedgeHidWorker;

typedef enum {
    FlipperWedgeHidWorkerModeUsb,
    FlipperWedgeHidWorkerModeBle,
} FlipperWedgeHidWorkerMode;

/** Allocate HID worker
 * Worker thread owns the HID interface lifecycle
 *
 * @return FlipperWedgeHidWorker instance
 */
FlipperWedgeHidWorker* flipper_wedge_hid_worker_alloc(void);

/** Free HID worker
 * Stops worker thread and cleans up HID interface
 *
 * @param worker FlipperWedgeHidWorker instance
 */
void flipper_wedge_hid_worker_free(FlipperWedgeHidWorker* worker);

/** Start HID worker with specified mode
 * Creates worker thread that initializes HID interface
 *
 * @param worker FlipperWedgeHidWorker instance
 * @param mode USB or BLE mode
 */
void flipper_wedge_hid_worker_start(FlipperWedgeHidWorker* worker, FlipperWedgeHidWorkerMode mode);

/** Stop HID worker
 * Signals worker thread to exit and deinit HID interface
 * Blocks until worker thread exits
 *
 * @param worker FlipperWedgeHidWorker instance
 */
void flipper_wedge_hid_worker_stop(FlipperWedgeHidWorker* worker);

/** Get HID instance from worker
 * Returns the HID interface managed by the worker
 *
 * @param worker FlipperWedgeHidWorker instance
 * @return FlipperWedgeHid instance
 */
FlipperWedgeHid* flipper_wedge_hid_worker_get_hid(FlipperWedgeHidWorker* worker);

/** Check if worker is running
 *
 * @param worker FlipperWedgeHidWorker instance
 * @return true if worker thread is active
 */
bool flipper_wedge_hid_worker_is_running(FlipperWedgeHidWorker* worker);
