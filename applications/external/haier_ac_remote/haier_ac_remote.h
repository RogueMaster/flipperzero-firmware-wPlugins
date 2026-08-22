#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "haier_state.h"
#include "haier_ir_protocol.h"
#include "views/haier_main_view.h"
#include "views/haier_extra_view.h"
#include "views/haier_setup_view.h"

// View IDs
typedef enum {
    HaierViewMain = 0,
    HaierViewExtra,
    HaierViewSetup,
} HaierViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    HaierMainView* main_view;
    HaierExtraView* extra_view;
    HaierSetupView* setup_view;

    // State
    HaierState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[HAIER_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[HAIER_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    HaierViewId current_view;
} HaierApp;
