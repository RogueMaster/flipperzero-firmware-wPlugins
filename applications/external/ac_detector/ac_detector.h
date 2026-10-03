#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "ac_decode.h"
#include "views/detector_view.h"

typedef enum {
    AcDetectorViewMain = 0,
} AcDetectorViewId;

typedef enum {
    AcDetectorEventRx = 0,
} AcDetectorCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    DetectorView* detector_view;

    InfraredWorker* ir_worker;

    /// Filled by the worker thread, drained on the application thread. A
    /// capture that lands while we are still decoding the previous one is
    /// dropped, which costs nothing - remotes send the same frame again.
    FuriMutex* rx_mutex;
    uint32_t rx_timings[AC_MAX_TIMINGS];
    size_t rx_count;

    FuriTimer* tick_timer;
} AcDetectorApp;
