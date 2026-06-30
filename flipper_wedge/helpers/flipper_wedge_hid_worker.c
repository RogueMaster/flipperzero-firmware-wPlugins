#include "flipper_wedge_hid_worker.h"
#include "flipper_wedge_debug.h"

#define TAG "FlipperWedgeHidWorker"

typedef enum {
    FlipperWedgeHidWorkerEventStop = (1 << 0),
} FlipperWedgeHidWorkerEvent;

struct FlipperWedgeHidWorker {
    FlipperWedgeHid* hid;
    FuriThread* thread;
    FlipperWedgeHidWorkerMode mode;
};

static int32_t flipper_wedge_hid_worker_thread(void* context) {
    FlipperWedgeHidWorker* worker = context;

    FURI_LOG_I(TAG, "Worker thread started, mode=%d", worker->mode);
    flipper_wedge_debug_log(TAG, "Worker thread starting HID init (mode=%d)", worker->mode);

    // Initialize HID interface in worker thread context
    if(worker->mode == FlipperWedgeHidWorkerModeUsb) {
        flipper_wedge_hid_init_usb(worker->hid);
    } else {
        flipper_wedge_hid_init_ble(worker->hid);
    }

    FURI_LOG_I(TAG, "Worker thread HID initialized, waiting for stop signal");
    flipper_wedge_debug_log(TAG, "Worker thread HID init complete, entering wait loop");

    // Wait for stop signal
    while(true) {
        uint32_t events = furi_thread_flags_wait(
            FlipperWedgeHidWorkerEventStop,
            FuriFlagWaitAny,
            FuriWaitForever);

        if(events & FlipperWedgeHidWorkerEventStop) {
            FURI_LOG_I(TAG, "Worker thread received stop signal");
            flipper_wedge_debug_log(TAG, "Worker thread stopping, deiniting HID");
            break;
        }
    }

    // Deinitialize HID interface in worker thread context
    if(worker->mode == FlipperWedgeHidWorkerModeUsb) {
        flipper_wedge_hid_deinit_usb(worker->hid);
    } else {
        flipper_wedge_hid_deinit_ble(worker->hid);
    }

    FURI_LOG_I(TAG, "Worker thread exiting");
    flipper_wedge_debug_log(TAG, "Worker thread HID deinit complete, exiting");

    return 0;
}

FlipperWedgeHidWorker* flipper_wedge_hid_worker_alloc(void) {
    FlipperWedgeHidWorker* worker = malloc(sizeof(FlipperWedgeHidWorker));

    worker->hid = flipper_wedge_hid_alloc();
    worker->thread = NULL;
    worker->mode = FlipperWedgeHidWorkerModeUsb;

    return worker;
}

void flipper_wedge_hid_worker_free(FlipperWedgeHidWorker* worker) {
    furi_assert(worker);

    // Stop worker if running
    if(worker->thread) {
        flipper_wedge_hid_worker_stop(worker);
    }

    flipper_wedge_hid_free(worker->hid);
    free(worker);
}

void flipper_wedge_hid_worker_start(FlipperWedgeHidWorker* worker, FlipperWedgeHidWorkerMode mode) {
    furi_assert(worker);
    furi_assert(!worker->thread);  // Don't start if already running

    FURI_LOG_I(TAG, "Starting worker thread with mode=%d", mode);
    flipper_wedge_debug_log(TAG, "Starting worker thread (mode=%d)", mode);

    worker->mode = mode;
    worker->thread = furi_thread_alloc_ex(
        "FlipperWedgeHidWorker",
        2048,  // Stack size
        flipper_wedge_hid_worker_thread,
        worker);

    furi_thread_start(worker->thread);

    // Small delay to let worker initialize
    furi_delay_ms(100);

    FURI_LOG_I(TAG, "Worker thread started");
}

void flipper_wedge_hid_worker_stop(FlipperWedgeHidWorker* worker) {
    furi_assert(worker);

    if(!worker->thread) {
        FURI_LOG_W(TAG, "Worker thread not running");
        return;
    }

    FURI_LOG_I(TAG, "Stopping worker thread");
    flipper_wedge_debug_log(TAG, "Signaling worker thread to stop");

    // Signal thread to stop
    furi_thread_flags_set(furi_thread_get_id(worker->thread), FlipperWedgeHidWorkerEventStop);

    // Wait for thread to exit
    FURI_LOG_D(TAG, "Waiting for worker thread to exit");
    furi_thread_join(worker->thread);

    // Free thread
    furi_thread_free(worker->thread);
    worker->thread = NULL;

    FURI_LOG_I(TAG, "Worker thread stopped");
    flipper_wedge_debug_log(TAG, "Worker thread stopped and cleaned up");
}

FlipperWedgeHid* flipper_wedge_hid_worker_get_hid(FlipperWedgeHidWorker* worker) {
    furi_assert(worker);
    return worker->hid;
}

bool flipper_wedge_hid_worker_is_running(FlipperWedgeHidWorker* worker) {
    furi_assert(worker);
    return (worker->thread != NULL);
}
