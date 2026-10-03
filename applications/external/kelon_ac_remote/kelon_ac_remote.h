#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "kelon_state.h"
#include "kelon_ir_protocol.h"
#include "views/kelon_main_view.h"
#include "views/kelon_extra_view.h"
#include "views/kelon_setup_view.h"

// View IDs
typedef enum {
    KelonViewMain = 0,
    KelonViewExtra,
    KelonViewSetup,
} KelonViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    KelonMainView* main_view;
    KelonExtraView* extra_view;
    KelonSetupView* setup_view;

    // State
    KelonState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[KELON_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[KELON_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    KelonViewId current_view;
} KelonApp;
