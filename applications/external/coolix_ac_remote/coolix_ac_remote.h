#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "coolix_state.h"
#include "coolix_ir_protocol.h"
#include "views/coolix_main_view.h"
#include "views/coolix_extra_view.h"
#include "views/coolix_setup_view.h"

// View IDs
typedef enum {
    CoolixViewMain = 0,
    CoolixViewExtra,
    CoolixViewSetup,
} CoolixViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    CoolixMainView* main_view;
    CoolixExtraView* extra_view;
    CoolixSetupView* setup_view;

    // State
    CoolixState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[COOLIX_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[24];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    CoolixViewId current_view;
} CoolixApp;
