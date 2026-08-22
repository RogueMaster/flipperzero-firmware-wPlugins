#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "toshiba_state.h"
#include "toshiba_ir_protocol.h"
#include "views/toshiba_main_view.h"
#include "views/toshiba_extra_view.h"
#include "views/toshiba_setup_view.h"

// View IDs
typedef enum {
    ToshibaViewMain = 0,
    ToshibaViewExtra,
    ToshibaViewSetup,
} ToshibaViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    ToshibaMainView* main_view;
    ToshibaExtraView* extra_view;
    ToshibaSetupView* setup_view;

    // State
    ToshibaState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[TOSHIBA_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[TOSHIBA_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    ToshibaViewId current_view;
} ToshibaApp;
