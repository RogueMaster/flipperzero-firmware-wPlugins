#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "htw_state.h"
#include "htw_ir_protocol.h"
#include "views/htw_main_view.h"
#include "views/htw_timer_view.h"
#include "views/htw_setup_view.h"

// View IDs
typedef enum {
    HtwViewMain = 0,
    HtwViewTimer,
    HtwViewSetup,
} HtwViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    HtwMainView* main_view;
    HtwTimerView* timer_view;
    HtwSetupView* setup_view;

    // State
    HtwState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[HTW_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    HtwViewId current_view;
} HtwApp;
