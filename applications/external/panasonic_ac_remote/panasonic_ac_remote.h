#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "panasonic_state.h"
#include "panasonic_ir_protocol.h"
#include "views/panasonic_main_view.h"
#include "views/panasonic_extra_view.h"
#include "views/panasonic_setup_view.h"

// View IDs
typedef enum {
    PanasonicViewMain = 0,
    PanasonicViewExtra,
    PanasonicViewSetup,
} PanasonicViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    PanasonicMainView* main_view;
    PanasonicExtraView* extra_view;
    PanasonicSetupView* setup_view;

    // State
    PanasonicState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[PANASONIC_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[PANASONIC_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    PanasonicViewId current_view;
} PanasonicApp;
