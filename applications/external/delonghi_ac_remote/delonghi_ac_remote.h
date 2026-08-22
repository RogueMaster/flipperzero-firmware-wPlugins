#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "delonghi_state.h"
#include "delonghi_ir_protocol.h"
#include "views/delonghi_main_view.h"
#include "views/delonghi_extra_view.h"
#include "views/delonghi_setup_view.h"

// View IDs
typedef enum {
    DelonghiViewMain = 0,
    DelonghiViewExtra,
    DelonghiViewSetup,
} DelonghiViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    DelonghiMainView* main_view;
    DelonghiExtraView* extra_view;
    DelonghiSetupView* setup_view;

    // State
    DelonghiState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[DELONGHI_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[DELONGHI_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    DelonghiViewId current_view;
} DelonghiApp;
