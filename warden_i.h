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

#include "warden_icons.h" // generated from icons/ by fbt

#include "helpers/card_reader.h"
#include "helpers/grader.h"
#include "views/scan_view.h"
#include "views/result_view.h"
#include "scenes/warden_scene.h"

#define WARDEN_VERSION "1.0"

typedef enum {
    WardenViewSubmenu,
    WardenViewScan,
    WardenViewResult,
    WardenViewSettings,
    WardenViewWidget,
} WardenViewId;

typedef enum {
    WardenCustomEventCardRead = 100, // scan worker found + read a card
    WardenCustomEventRescan, // user asked to grade another
    WardenCustomEventDetails, // user opened the breakdown
} WardenCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;
    ScanView* scan_view;
    ResultView* result_view;

    CardReader* reader;

    /* settings */
    bool sound;
    bool vibro;
    bool led;

    /* the current verdict */
    CardReading reading;
    CardGrade grade;
    bool have_result;
} WardenApp;

/* feedback (defined in warden.c), all gated by settings */
void warden_notify_graded(WardenApp* app, RiskBand band);
void warden_notify_scan_blip(WardenApp* app);
