#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "faraday_icons.h" // generated from icons/ by fbt

#include "helpers/fdy_subghz.h"
#include "helpers/fdy_nfc.h"
#include "helpers/fdy_grade.h"
#include "helpers/fdy_store.h" // also defines FaradaySettings
#include "views/meter_view.h"
#include "views/hunt_view.h"
#include "views/splash_view.h"
#include "scenes/faraday_scene.h"

#define FARADAY_VERSION "1.2"

typedef enum {
    FaradayViewSplash,
    FaradayViewSubmenu,
    FaradayViewMeter,
    FaradayViewHunt,
    FaradayViewSettings,
    FaradayViewAbout,
} FaradayViewId;

typedef enum {
    FaradayCustomEventOk = 100, // OK pressed on the meter / hunt screen
} FaradayCustomEvent;

/** State of the running two-step test, shared by both radio scenes. */
typedef struct {
    FdyPhase phase;
    bool have_base;
    bool have_shield;
    int16_t base_value; // real units (dBm or %)
    int16_t shield_value;
    uint8_t base_norm; // 0..100 for the bars
    uint8_t shield_norm;
    bool shield_floored; // shielded reading was buried in the noise floor
    int16_t atten; // dB attenuation, or % of field blocked
    bool atten_floored; // attenuation is a ">=" lower bound
    uint8_t rating; // FdyRating
} FdyTest;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    MeterView* meter_view;
    HuntView* hunt_view;
    SplashView* splash_view;

    FdySubGhz* subghz;
    FdyNfc* nfc;

    FaradaySettings settings;
    FdyTest test;

    uint32_t last_click_tick; // paces the leak-hunt geiger clicks
    bool splash_done; // the intro plays once per launch
    uint8_t splash_ticks; // intro auto-advance counter
} FaradayApp;

/* test-flow helpers (defined in faraday.c) */
void fdy_test_reset(FdyTest* t);

/* feedback (defined in faraday.c, gated by settings) */
void faraday_notify_lock(FaradayApp* app); // a reference was captured
void faraday_notify_reject(FaradayApp* app); // nothing to capture yet
void faraday_notify_verdict(FaradayApp* app, uint8_t rating);
void faraday_notify_click(FaradayApp* app); // single leak-hunt geiger tick

/* Log a finished test (defined in faraday.c). */
void faraday_log_result(FaradayApp* app, bool is_nfc, uint32_t frequency);
