#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "fujitsu_state.h"
#include "fujitsu_ir_protocol.h"
#include "views/fujitsu_main_view.h"
#include "views/fujitsu_extra_view.h"
#include "views/fujitsu_setup_view.h"

// View IDs
typedef enum {
    FujitsuViewMain = 0,
    FujitsuViewExtra,
    FujitsuViewSetup,
} FujitsuViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    FujitsuMainView* main_view;
    FujitsuExtraView* extra_view;
    FujitsuSetupView* setup_view;

    // State
    FujitsuState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[FUJITSU_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[FUJITSU_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    FujitsuViewId current_view;
} FujitsuApp;
