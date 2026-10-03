#include "ac_detector.h"

#include <furi_hal_infrared.h>

#define TAG "AcDetector"

/// Drives the marquee and the receive marker.
#define TICK_PERIOD_MS 250

// --------------------------------------------------------------- ir receive

/// Runs on the worker thread. Copy and get out; decoding happens on the
/// application thread where a few milliseconds of work costs nothing.
static void ac_detector_rx_callback(void* context, InfraredWorkerSignal* signal) {
    AcDetectorApp* app = context;

    if(infrared_worker_signal_is_decoded(signal)) {
        // We asked for raw timings, so this should not happen. Ignore it
        // rather than reading the wrong half of the union.
        return;
    }

    const uint32_t* timings = NULL;
    size_t count = 0;
    infrared_worker_get_raw_signal(signal, &timings, &count);
    if(!timings || count == 0) return;

    if(count > AC_MAX_TIMINGS) count = AC_MAX_TIMINGS;

    if(furi_mutex_acquire(app->rx_mutex, 0) != FuriStatusOk) return;
    memcpy(app->rx_timings, timings, count * sizeof(uint32_t));
    app->rx_count = count;
    furi_mutex_release(app->rx_mutex);

    view_dispatcher_send_custom_event(app->view_dispatcher, AcDetectorEventRx);
}

static void ac_detector_handle_rx(AcDetectorApp* app) {
    AcDetection d;
    bool fresh;

    // Decode in place, under the lock. The worker's callback takes the mutex
    // with a zero timeout, so a capture that lands mid-decode is dropped
    // instead of stalling the worker thread - and dropping one costs nothing,
    // because remotes send the same frame again a few milliseconds later.
    furi_mutex_acquire(app->rx_mutex, FuriWaitForever);
    size_t count = app->rx_count;
    app->rx_count = 0;
    fresh = count ? ac_decode(app->rx_timings, count, &d) : false;
    furi_mutex_release(app->rx_mutex);

    if(!count) return;

    if(!fresh) {
        // Lamp flicker, a reflection, a truncated frame. Leave the screen
        // showing whatever was last identified.
        detector_view_note_noise(app->detector_view);
        return;
    }

    detector_view_set_result(app->detector_view, &d);
    notification_message(
        app->notifications,
        d.kind == AcResultMatch ? &sequence_blink_green_100 : &sequence_blink_blue_100);
}

// ------------------------------------------------------------------ plumbing

static bool ac_detector_custom_event_callback(void* context, uint32_t event) {
    AcDetectorApp* app = context;
    if(event == AcDetectorEventRx) {
        ac_detector_handle_rx(app);
        return true;
    }
    return false;
}

static bool ac_detector_navigation_callback(void* context) {
    AcDetectorApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static void ac_detector_on_back(void* context) {
    AcDetectorApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
}

static void ac_detector_tick_callback(void* context) {
    AcDetectorApp* app = context;
    detector_view_tick(app->detector_view);
}

// ----------------------------------------------------------------- lifecycle

static AcDetectorApp* ac_detector_app_alloc(void) {
    AcDetectorApp* app = malloc(sizeof(AcDetectorApp));
    memset(app, 0, sizeof(AcDetectorApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ac_detector_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ac_detector_navigation_callback);

    app->detector_view = detector_view_alloc();
    detector_view_set_back_callback(app->detector_view, ac_detector_on_back, app);
    view_dispatcher_add_view(
        app->view_dispatcher, AcDetectorViewMain, detector_view_get_view(app->detector_view));

    app->rx_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(app->ir_worker, ac_detector_rx_callback, app);
    // Raw timings, not decoded consumer-IR messages: air conditioner frames
    // are far longer than anything the firmware's decoders know about.
    infrared_worker_rx_enable_signal_decoding(app->ir_worker, false);
    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, false);

    app->tick_timer = furi_timer_alloc(ac_detector_tick_callback, FuriTimerTypePeriodic, app);
    return app;
}

static void ac_detector_app_free(AcDetectorApp* app) {
    furi_assert(app);

    furi_timer_free(app->tick_timer);
    infrared_worker_free(app->ir_worker);
    furi_mutex_free(app->rx_mutex);

    view_dispatcher_remove_view(app->view_dispatcher, AcDetectorViewMain);
    detector_view_free(app->detector_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t ac_detector_app(void* p) {
    UNUSED(p);

    AcDetectorApp* app = ac_detector_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, AcDetectorViewMain);

    infrared_worker_rx_start(app->ir_worker);
    furi_timer_start(app->tick_timer, furi_ms_to_ticks(TICK_PERIOD_MS));

    view_dispatcher_run(app->view_dispatcher);

    furi_timer_stop(app->tick_timer);
    infrared_worker_rx_stop(app->ir_worker);

    ac_detector_app_free(app);
    return 0;
}
