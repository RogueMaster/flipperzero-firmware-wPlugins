#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "kelvinator_state.h"
#include "kelvinator_ir_protocol.h"
#include "views/kelvinator_main_view.h"
#include "views/kelvinator_extra_view.h"
#include "views/kelvinator_setup_view.h"

// View IDs
typedef enum {
    KelvinatorViewMain = 0,
    KelvinatorViewExtra,
    KelvinatorViewSetup,
} KelvinatorViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    KelvinatorMainView* main_view;
    KelvinatorExtraView* extra_view;
    KelvinatorSetupView* setup_view;

    // State
    KelvinatorState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[KELVINATOR_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[KELVINATOR_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    KelvinatorViewId current_view;
} KelvinatorApp;
