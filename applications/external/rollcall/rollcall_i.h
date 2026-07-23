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

#include "rollcall_icons.h" // generated from icons/ by fbt

#include "helpers/rc_radio.h"
#include "helpers/analyzer.h"
#include "views/capture_view.h"
#include "views/verdict_view.h"
#include "scenes/rollcall_scene.h"

#define ROLLCALL_VERSION "1.0"

typedef enum {
    RollCallViewSubmenu,
    RollCallViewCapture,
    RollCallViewVerdict,
    RollCallViewSettings,
    RollCallViewWidget,
} RollCallViewId;

typedef enum {
    RollCallCustomEventCapture = 100, // worker registered a new press
    RollCallCustomEventFinish, // done capturing -> analyze
    RollCallCustomEventDetails, // open the per-press breakdown
    RollCallCustomEventRescan, // run the check again
} RollCallCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;
    CaptureView* capture_view;
    VerdictView* verdict_view;

    RcRadio* radio;

    /* settings */
    uint8_t band_idx; // index into rc_bands
    uint8_t mod_idx; // index into rc_mods
    uint8_t target; // presses to auto-finish (2..6)
    bool sound;
    bool vibro;
    bool led;

    /* current run */
    RcCapture captures[RC_MAX_CAPTURES];
    uint8_t capture_count;
    RcVerdict verdict;
    bool have_verdict;
} RollCallApp;

/* feedback (defined in rollcall.c), all gated by settings */
void rollcall_notify_verdict(RollCallApp* app, RcHealth health);
void rollcall_notify_capture(RollCallApp* app);
