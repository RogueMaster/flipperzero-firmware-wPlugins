#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "lg_state.h"
#include "lg_ir_protocol.h"
#include "views/lg_main_view.h"
#include "views/lg_extra_view.h"
#include "views/lg_setup_view.h"

// View IDs
typedef enum {
    LgViewMain = 0,
    LgViewExtra,
    LgViewSetup,
} LgViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    LgMainView* main_view;
    LgExtraView* extra_view;
    LgSetupView* setup_view;

    // State
    LgState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[LG_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[LG_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    LgViewId current_view;
} LgApp;
