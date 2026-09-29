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

#include "ghosttag_icons.h" // generated from icons/ by fbt

#include "helpers/ble_signatures.h"
#include "helpers/settings.h"
#include "helpers/tracker_db.h"
#include "helpers/uart_link.h"
#include "helpers/demo_source.h"
#include "helpers/air_check.h"
#include "helpers/session_log.h"
#include "views/splash_view.h"
#include "views/radar_view.h"
#include "views/air_view.h"
#include "views/device_list_view.h"
#include "views/device_detail_view.h"
#include "views/alert_view.h"
#include "scenes/ghosttag_scene.h"

#define GHOSTTAG_VERSION "2.0"

/*
 * Air Check: measuring BLE advertising-band energy with the Flipper's own
 * radio. OFF by default, because on official firmware it does not work.
 *
 * What was tried, on real hardware, on API 87:
 *   - furi_hal_bt_ensure_c2_mode(BleGlueC2ModeStack) returns TRUE, so the
 *     second core is up and running the radio stack.
 *   - bt_disconnect() + furi_hal_bt_stop_advertising() to free the radio.
 *   - furi_hal_bt_start_rx(channel)        -> furi_hal_bt_get_rssi() == 0.0
 *   - furi_hal_bt_start_packet_rx(ch, 1M)  -> furi_hal_bt_get_rssi() == 0.0
 *
 * A return of exactly 0 is how this stack reports a FAILED read, and it does
 * it for every sample on both paths, so the app gets no energy measurement at
 * all. The code is kept because it is correct as written and would start
 * working the day a firmware hands an application a real RSSI - but a menu
 * entry that always reads NO READING teaches a new user that the app is
 * broken, which is the precise problem this release exists to fix.
 *
 * Set this to 1 to put it back in the menu and test it against a firmware
 * that may behave differently.
 */
#ifndef GHOSTTAG_ENABLE_AIR_CHECK
#define GHOSTTAG_ENABLE_AIR_CHECK 0
#endif

/** How long without a byte from the board before the link is called dead. */
#define GHOSTTAG_ESP_TIMEOUT_MS 4000

typedef enum {
    GhostTagViewSplash,
    GhostTagViewSubmenu,
    GhostTagViewRadar,
    GhostTagViewAir,
    GhostTagViewDeviceList,
    GhostTagViewDeviceDetail,
    GhostTagViewAlert,
    GhostTagViewSettings,
    GhostTagViewAbout,
} GhostTagViewId;

/*
 * Custom events start above the submenu index range so a stray menu index can
 * never be mistaken for one of them.
 */
typedef enum {
    GhostTagCustomEventSplashDone = 100,
    GhostTagCustomEventScanOpenList,
    GhostTagCustomEventScanOpenHelp,
    GhostTagCustomEventOpenDetail,
} GhostTagCustomEvent;

/*
 * There is deliberately no "new follower" custom event.
 *
 * Posting one from the radio worker meant the worker could block inside
 * view_dispatcher_send_custom_event when the dispatcher's queue was full -
 * and the thread that drains that queue is the GUI thread, which is the same
 * thread that blocks in furi_thread_join while tearing the link down. That is
 * a genuine two-thread deadlock, and it needed a reboot to clear.
 *
 * The worker now only touches the database. Every scene that can be on screen
 * when an alert fires polls for one on its 100 ms tick instead, so nothing
 * crosses a thread boundary and no alert can be lost to a full queue.
 */

/** Where detections are coming from. Drives both behaviour and what is drawn. */
typedef enum {
    GhostTagSourceNone,
    GhostTagSourceEsp32, /* the real radio */
    GhostTagSourceDemo, /* a scripted simulation, stamped DEMO on every screen */
} GhostTagSource;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    SplashView* splash_view;
    RadarView* radar_view;
    AirView* air_view;
    DeviceListView* device_list_view;
    DeviceDetailView* device_detail_view;
    AlertView* alert_view;

    TrackerDb* db;
    UartLink* uart;
    DemoSource* demo;
    AirCheck* air;
    SessionLog* log;

    /* One shared snapshot buffer for every screen that needs the sorted
     * detection list. A TrackerRecord is ~44 bytes and there are 48 of them,
     * so a local array costs 2.1 KB of stack - and three separate scene
     * handlers were each taking that hit on the GUI thread. Heap, once.
     * Safe to share: scene handlers all run on the GUI thread and never nest. */
    TrackerRecord* scratch;

    GhostTagSettings settings;
    GhostTagSource source;
    /* Which source the CURRENT detections came from. Re-entering the same kind
     * of hunt continues the session; switching between real and simulated
     * wipes it, because mixing invented traffic into real findings is the one
     * thing this app must never do. */
    GhostTagSource db_source;

    /* The record the detail screen is showing. Separate from the alert's copy:
     * sharing one field let an alert arriving mid-navigation swap the device
     * under the user's cursor. */
    TrackerRecord detail_record;
    TrackerRecord alert_record;

    bool esp_connected;
    bool esp_ever_seen; /* have we EVER heard from a board this session? */
    char esp_version[16];

    bool backlight_forced;
} GhostTagApp;

/** True while a detection source is live, whichever one it is. */
bool ghosttag_is_hunting(GhostTagApp* app);

/** Start/stop whichever source @c app->source names. */
void ghosttag_source_start(GhostTagApp* app, GhostTagSource source);
void ghosttag_source_stop(GhostTagApp* app);

/** Drop every detection and start the session over. */
void ghosttag_clear_detections(GhostTagApp* app);

/** The dwell a tracker must survive to be called a follower, for this source. */
uint32_t ghosttag_follow_threshold_ms(GhostTagApp* app);

/** Fired on the GUI thread to play the configured alert feedback. */
void ghosttag_notify_alert(GhostTagApp* app);

/** Hold the backlight on for a hunt, or release it. */
void ghosttag_backlight_hold(GhostTagApp* app, bool hold);

/**
 * Take one pending alert, if there is one, and push the alert scene.
 *
 * Call this from the tick handler of every scene that can be on screen during
 * a hunt. Returns true if an alert was raised.
 */
bool ghosttag_poll_alert(GhostTagApp* app);

/** Re-evaluate the ESP32 link state from the board's last line. */
void ghosttag_update_link(GhostTagApp* app);
