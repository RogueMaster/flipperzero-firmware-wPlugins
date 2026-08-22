#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "neoclima_state.h"
#include "neoclima_ir_protocol.h"
#include "views/neoclima_main_view.h"
#include "views/neoclima_extra_view.h"
#include "views/neoclima_setup_view.h"

// View IDs
typedef enum {
    NeoclimaViewMain = 0,
    NeoclimaViewExtra,
    NeoclimaViewSetup,
} NeoclimaViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    NeoclimaMainView* main_view;
    NeoclimaExtraView* extra_view;
    NeoclimaSetupView* setup_view;

    // State
    NeoclimaState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[NEOCLIMA_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[NEOCLIMA_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    NeoclimaViewId current_view;
} NeoclimaApp;
