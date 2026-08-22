#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "midea_state.h"
#include "midea_ir_protocol.h"
#include "views/midea_main_view.h"
#include "views/midea_extra_view.h"
#include "views/midea_setup_view.h"

// View IDs
typedef enum {
    MideaViewMain = 0,
    MideaViewExtra,
    MideaViewSetup,
} MideaViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    MideaMainView* main_view;
    MideaExtraView* extra_view;
    MideaSetupView* setup_view;

    // State
    MideaState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[MIDEA_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[MIDEA_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    MideaViewId current_view;
} MideaApp;
