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

#include "nyx_icons.h" // generated from icons/ by fbt

#include "helpers/ir_sense.h"
#include "views/sweep_view.h"
#include "views/probe_view.h"
#include "scenes/nyx_scene.h"

#define NYX_VERSION "1.0"

typedef enum {
    NyxViewSubmenu,
    NyxViewSweep,
    NyxViewProbe,
    NyxViewSettings,
    NyxViewAbout,
} NyxViewId;

typedef enum {
    NyxCustomEventReset = 100, // OK on the sweep screen clears peak/hits
    NyxCustomEventRenull, // long OK re-captures the ambient baseline
} NyxCustomEvent;

typedef struct {
    uint8_t mode_index; // 0 Auto, 1 Onboard, 2 Probe  (IrSenseMode order)
    uint8_t sensitivity_index; // 0 High, 1 Medium, 2 Low
    uint8_t probe_pin_index; // index into ir_sense_probe_pins()
    bool sound;
    bool vibro;
    bool led;
} NyxSettings;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    SweepView* sweep_view;
    ProbeView* probe_view;

    IrSense* sense;

    NyxSettings settings;

    bool was_present; // edge tracking for the "found" alert
    uint32_t last_click_tick; // paces the geiger clicks
} NyxApp;

/* settings labels (defined in nyx_scene_settings.c) */
const char* nyx_settings_mode_label(uint8_t index);
const char* nyx_settings_sensitivity_label(uint8_t index);

/* alert feedback (defined in nyx.c) */
void nyx_notify_found(NyxApp* app); // an emitter just appeared
void nyx_notify_gone(NyxApp* app); // it dropped out
void nyx_notify_click(NyxApp* app); // single geiger tick
void nyx_notify_present_led(NyxApp* app); // steady "locked" blink
