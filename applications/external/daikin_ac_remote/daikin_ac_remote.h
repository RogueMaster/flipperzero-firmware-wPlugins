#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "daikin_state.h"
#include "daikin_ir_protocol.h"
#include "views/daikin_main_view.h"
#include "views/daikin_extra_view.h"
#include "views/daikin_setup_view.h"

// View IDs
typedef enum {
    DaikinViewMain = 0,
    DaikinViewExtra,
    DaikinViewSetup,
} DaikinViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    DaikinMainView* main_view;
    DaikinExtraView* extra_view;
    DaikinSetupView* setup_view;

    // State
    DaikinState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[DAIKIN_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[DAIKIN_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    DaikinViewId current_view;
} DaikinApp;
