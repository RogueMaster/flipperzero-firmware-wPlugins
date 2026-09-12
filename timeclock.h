// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// Time Clock - Flipper Zero app for personal work time tracking using NFC/RFID
// badge identification.
//
// The user chooses the reader (NFC or LF RFID) in Settings. The app auto-detects
// whatever the chosen radio supports and identifies a badge by its UID. It only
// reads the identifier: it does not emulate badges and does not try to bypass
// any authentication system. Use only with badges/systems you are authorized to
// use.
// =============================================================================

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>

#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "timeclock_storage.h"
#include "timeclock_pin.h"
#include "timeclock_reader.h"
#include "timeclock_i18n.h"
#include "views/pin_view.h"
#include "views/work_view.h"
#include "scenes/timeclock_scene.h"

#define TC_TAG "TimeClock"

// The limits and the data model (TcEventType, Badge, TcConfig) are defined in
// timeclock_storage.h, included above, since storage owns their persistence.
// Only the PIN-related constants live here.
#define TC_PIN_LEN 4
#define TC_MAX_PIN_ATTEMPTS 5

const char* tc_event_str(TcEventType type);

// ---- Views (ids for the ViewDispatcher) ------------------------------------
typedef enum {
    TimeClockViewSubmenu,
    TimeClockViewTextInput,
    TimeClockViewTextBox,
    TimeClockViewWidget,
    TimeClockViewPopup,
    TimeClockViewPin,
    TimeClockViewWork,
} TimeClockView;

// ---- ViewDispatcher custom events ------------------------------------------
typedef enum {
    TimeClockCustomEventBadgeFound = 100, // known badge recognized
    TimeClockCustomEventBadgeUnknown, // badge not registered
    TimeClockCustomEventScanError, // read error
    TimeClockCustomEventPinEntered, // PIN submitted from the PinView
    TimeClockCustomEventWorkExit, // Back pressed in Work mode (needs PIN)
} TimeClockCustomEvent;

// ---- PIN scene mode --------------------------------------------------------
typedef enum {
    TcPinModeUnlock, // unlock at startup
    TcPinModeSetNew, // set PIN: first entry
    TcPinModeConfirmNew, // set PIN: confirmation
    TcPinModeVerifyOld, // change PIN: verify the current one
    TcPinModeVerifyExit, // protected exit from the app
    TcPinModeVerifyExitWork, // protected exit from Work mode back to the menu
} TcPinMode;

// ---- Purpose of a scan -----------------------------------------------------
typedef enum {
    TcScanPunch, // clock in/out an existing collaborator
    TcScanRegister, // register a new collaborator (name only, no punch)
    TcScanReplace, // reassign a new chip to an existing collaborator
} TcScanPurpose;

// ---- Application state -----------------------------------------------------
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    // GUI modules shared across scenes
    Submenu* submenu;
    TextInput* text_input;
    TextBox* text_box;
    Widget* widget;
    Popup* popup;
    PinView* pin_view;
    WorkView* work_view;

    // Content buffer for the TextBox (history / today)
    FuriString* text_store;

    // Configuration
    TcConfig config;

    // In-memory badge list
    Badge badges[TC_MAX_BADGES];
    size_t badge_count;

    // Scan / selection state
    char scanned_uid[TC_UID_STR_MAX];
    char scanned_tech[TC_TECH_MAX];
    int found_index; // index of the recognized badge, -1 if none
    int selected_index; // badge selected from the list
    int replace_index; // collaborator whose chip is being reassigned, -1 if none
    TcScanPurpose scan_purpose; // what the current scan is for

    // Name input buffer
    char name_buf[TC_NAME_MAX];

    // PIN state
    TcPinMode pin_mode;
    char pin_new[TC_PIN_LEN + 1]; // PIN being set
} TimeClock;

// Reload the badge list from disk into app->badges.
void timeclock_reload_badges(TimeClock* app);

// Return the index of the badge with that UID, or -1.
int timeclock_find_badge(TimeClock* app, const char* uid);

// Short feedback notifications (haptic/LED).
void timeclock_notify_success(TimeClock* app);
void timeclock_notify_error(TimeClock* app);

// Punch feedback: distinct sound/vibro/LED for IN vs OUT, each honoring its
// own on/off setting (config.sound_enabled / vibro_enabled / led_enabled).
void timeclock_notify_punch(TimeClock* app, TcEventType type);

// Record an automatic alternating punch (IN if last was OUT/none, else OUT) for
// the badge at index, append it to history, persist, and play feedback.
// Returns the recorded event type.
TcEventType timeclock_record_punch(TimeClock* app, int badge_index);
