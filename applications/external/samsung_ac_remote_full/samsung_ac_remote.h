#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "samsung_state.h"
#include "samsung_ir_protocol.h"
#include "views/samsung_main_view.h"
#include "views/samsung_extra_view.h"
#include "views/samsung_setup_view.h"

// View IDs
typedef enum {
    SamsungViewMain = 0,
    SamsungViewExtra,
    SamsungViewSetup,
} SamsungViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    SamsungMainView* main_view;
    SamsungExtraView* extra_view;
    SamsungSetupView* setup_view;

    // State
    SamsungState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[SAMSUNG_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[SAMSUNG_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    SamsungViewId current_view;
} SamsungApp;
