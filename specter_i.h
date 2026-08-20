#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "specter_icons.h" // generated from icons/ by fbt

#include "helpers/emitter_classify.h"
#include "helpers/field_detector.h"
#include "helpers/specter_log.h"
#include "helpers/specter_settings.h"
#include "helpers/survey_verdict.h"
#include "views/fingerprint_view.h"
#include "views/survey_view.h"
#include "views/sweep_view.h"
#include "views/watch_view.h"
#include "scenes/specter_scene.h"

#define SPECTER_VERSION "2.7"

/* How long the noise-floor calibration listens for, in milliseconds. */
#define SPECTER_CALIBRATE_MS 3000u

typedef enum {
    SpecterViewSubmenu,
    SpecterViewSweep,
    SpecterViewFingerprint,
    SpecterViewSurvey,
    SpecterViewWatch,
    SpecterViewTextBox,
    SpecterViewSettings,
    SpecterViewWidget, // shared by About and the clear-logbook confirmation
} SpecterViewId;

typedef enum {
    SpecterCustomEventReset = 100, // OK on the sweep screen clears peak/contacts
    SpecterCustomEventSweepLog, // long OK on the sweep screen logs the reading
    SpecterCustomEventCalibrate, // LEFT on the sweep screen samples the noise floor
    SpecterCustomEventFingerprintSave, // OK on the fingerprint screen logs the finding
    SpecterCustomEventFingerprintReset, // long OK restarts the measurement
    SpecterCustomEventSurveyRestart, // OK re-runs the survey
    SpecterCustomEventWatchReset, // OK re-arms the watch
} SpecterCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;
    TextBox* text_box;
    FuriString* text_box_store;

    SweepView* sweep_view;
    FingerprintView* fingerprint_view;
    SurveyView* survey_view;
    WatchView* watch_view;

    FieldDetector* detector;

    SpecterSettings settings;

    bool reader_active; // edge tracking for the "reader found" alert
    uint32_t last_click_tick; // paces the geiger clicks
    uint32_t last_found_tick; // floor on how often the found alert may fire
    uint32_t last_wake_tick; // floor on how often we may wake the screen
    bool stealth_engaged; // backlight currently forced dark
    bool settings_dirty; // settings changed while the radio is busy; save on exit
} SpecterApp;

/* alert feedback (defined in specter.c) - each is a no-op when the matching
 * setting is off, and the light/screen ones also yield to stealth mode */
void specter_notify_found(SpecterApp* app); // reader just appeared
void specter_notify_gone(SpecterApp* app); // reader left
void specter_notify_click(SpecterApp* app); // single geiger tick
void specter_notify_present_led(SpecterApp* app); // steady "locked" LED blink
void specter_notify_saved(SpecterApp* app); // a logbook write landed
void specter_notify_wake(SpecterApp* app); // pull the backlight on (watch mode)

/* Stealth mode: hold the backlight dark for the duration of a sweep so the
 * Flipper does not glow while you are the one doing the looking. */
void specter_stealth_enter(SpecterApp* app);
void specter_stealth_exit(SpecterApp* app);

/* Apply the current sensitivity setting to the detector. */
void specter_apply_threshold(SpecterApp* app);
