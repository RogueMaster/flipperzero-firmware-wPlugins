#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>

#include "goodweather_state.h"
#include "goodweather_ir_protocol.h"
#include "views/goodweather_main_view.h"
#include "views/goodweather_extra_view.h"
#include "views/goodweather_setup_view.h"

// View IDs
typedef enum {
    GoodweatherViewMain = 0,
    GoodweatherViewExtra,
    GoodweatherViewSetup,
} GoodweatherViewId;

// Main application structure
typedef struct {
    // System
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    GoodweatherMainView* main_view;
    GoodweatherExtraView* extra_view;
    GoodweatherSetupView* setup_view;

    // State
    GoodweatherState* state;

    // IR
    InfraredWorker* ir_worker;
    uint32_t ir_timings[GOODWEATHER_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    // Payload of the most recent transmission, shown on the Extra screen.
    // The Extra view holds a pointer to this, so it is always current.
    char last_sent[GOODWEATHER_CODE_STR_LEN];

    // Animation timer
    FuriTimer* send_timer;
    bool is_sending;
    uint8_t anim_frame;

    // IR stop timer (non-blocking TX stop)
    FuriTimer* ir_tx_stop_timer;

    // Current view tracking
    GoodweatherViewId current_view;
} GoodweatherApp;
