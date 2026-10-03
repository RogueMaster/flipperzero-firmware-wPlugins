#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "mitsubishi_state.h"
#include "mitsubishi_ir_protocol.h"
#include "views/mitsubishi_main_view.h"
#include "views/mitsubishi_extra_view.h"
#include "views/mitsubishi_setup_view.h"

// View IDs
typedef enum {
    MitsubishiViewMain = 0,
    MitsubishiViewExtra,
    MitsubishiViewSetup,
} MitsubishiViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    MitsubishiMainView* main_view;
    MitsubishiExtraView* extra_view;
    MitsubishiSetupView* setup_view;

    // State
    MitsubishiState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[MITSUBISHI_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[MITSUBISHI_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    MitsubishiViewId current_view;
} MitsubishiApp;
