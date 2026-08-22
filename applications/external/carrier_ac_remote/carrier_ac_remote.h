#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "carrier_state.h"
#include "carrier_ir_protocol.h"
#include "views/carrier_main_view.h"
#include "views/carrier_extra_view.h"
#include "views/carrier_setup_view.h"

// View IDs
typedef enum {
    CarrierViewMain = 0,
    CarrierViewExtra,
    CarrierViewSetup,
} CarrierViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    CarrierMainView* main_view;
    CarrierExtraView* extra_view;
    CarrierSetupView* setup_view;

    // State
    CarrierState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[CARRIER_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[CARRIER_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    CarrierViewId current_view;
} CarrierApp;
