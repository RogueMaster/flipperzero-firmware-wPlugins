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

#include "specter_icons.h" // generated from icons/ by fbt

#include "helpers/field_detector.h"
#include "views/sweep_view.h"
#include "scenes/specter_scene.h"

#define SPECTER_VERSION "1.0"

typedef enum {
    SpecterViewSubmenu,
    SpecterViewSweep,
    SpecterViewSettings,
    SpecterViewAbout,
} SpecterViewId;

typedef enum {
    SpecterCustomEventReset = 100, // OK on the sweep screen clears peak/contacts
} SpecterCustomEvent;

typedef struct {
    uint8_t sensitivity_index; // 0 High, 1 Medium, 2 Low
    bool sound;
    bool vibro;
    bool led;
} SpecterSettings;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    SweepView* sweep_view;

    FieldDetector* detector;

    SpecterSettings settings;

    bool reader_active; // edge tracking for the "reader found" alert
    uint32_t last_click_tick; // paces the geiger clicks
} SpecterApp;

/* settings (defined in specter_scene_settings.c) */
uint8_t specter_settings_threshold(const SpecterSettings* s);
const char* specter_settings_sensitivity_label(uint8_t index);

/* alert feedback (defined in specter.c) */
void specter_notify_found(SpecterApp* app); // reader just appeared
void specter_notify_gone(SpecterApp* app); // reader left
void specter_notify_click(SpecterApp* app); // single geiger tick
void specter_notify_present_led(SpecterApp* app); // steady "locked" LED blink
