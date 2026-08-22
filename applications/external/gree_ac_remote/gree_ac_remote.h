#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "gree_state.h"
#include "gree_ir_protocol.h"
#include "views/gree_main_view.h"
#include "views/gree_extra_view.h"
#include "views/gree_setup_view.h"

// View IDs
typedef enum {
    GreeViewMain = 0,
    GreeViewExtra,
    GreeViewSetup,
} GreeViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    GreeMainView* main_view;
    GreeExtraView* extra_view;
    GreeSetupView* setup_view;

    // State
    GreeState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[GREE_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[GREE_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    GreeViewId current_view;
} GreeApp;
