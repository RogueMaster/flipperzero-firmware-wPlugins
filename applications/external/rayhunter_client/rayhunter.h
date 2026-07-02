#pragma once

/**
 * rayhunter.h — Shared types and app state for Ray Hunter Client.
 *
 * Connects the Flipper to an ESP32 WiFi Dev Board (Marauder) via UART,
 * then polls the EFF Ray Hunter daemon on the tethered Orbic RC400L hotspot.
 * Displays real-time IMSI catcher anomaly detection status on the Flipper
 * screen.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>

#include "rayhunter_uart.h"

/* =========================================================================
 * Constants
 * ========================================================================= */

#define RH_HOST_LEN    32
#define RH_MESSAGE_LEN 128
#define RH_STATUS_LEN  64

/* Poll interval options (seconds). */
#define RH_POLL_INTERVALS_COUNT 5
static const uint32_t RH_POLL_INTERVALS[RH_POLL_INTERVALS_COUNT] = {2, 5, 10, 30, 60};

/* Settings: IP octet count for the naive IP text cycling. */
#define RH_PORT_MIN 1024
#define RH_PORT_MAX 65535

/* =========================================================================
 * Enums
 * ========================================================================= */

/** Severity levels parsed from the Ray Hunter NDJSON stream. */
typedef enum {
    RhThreatNone, /* No data received yet                  */
    RhThreatClean, /* "Informational" — all clear           */
    RhThreatLow, /* "Low" severity event                  */
    RhThreatMedium, /* "Medium" severity event               */
    RhThreatHigh, /* "High" — IMSI catcher suspected!      */
} RhThreatLevel;

/** View IDs registered with the ViewDispatcher. */
typedef enum {
    RhViewMain = 0,
    RhViewSettings,
    RhViewAbout,
} RhView;

/** Custom events sent via view_dispatcher_send_custom_event(). */
typedef enum {
    RhEventStatusUpdate = 0, /* Worker pushed a new RhStatus — redraw main view */
    RhEventHighThreat = 1, /* High severity detected — vibrate + blink        */
} RhCustomEvent;

/* =========================================================================
 * Data structures
 * ========================================================================= */

typedef struct {
    char host[RH_HOST_LEN]; /* e.g. "192.168.1.1"             */
    uint16_t port; /* default 8080                    */
    uint32_t poll_interval_s; /* seconds between polls; default 5 */
} RhConfig;

/** Live status updated by the worker, read by the draw callback via model. */
typedef struct {
    RhThreatLevel threat;
    char last_message[RH_MESSAGE_LEN]; /* e.g. "Cell suggested use of null cipher"
                                      */
    char status_text[RH_STATUS_LEN]; /* e.g. "Recording: active"                */
    uint32_t last_update_tick;
    bool connected; /* ESP32 has responded at least once       */
    uint32_t packet_count;
    uint32_t warning_count;
} RhStatus;

/** View model embedded inside the main custom View (locked by
 * ViewModelTypeLocking). */
typedef struct {
    RhStatus status;
    RhConfig config;
} RhMainModel;

/* =========================================================================
 * App state (heap-allocated)
 * ========================================================================= */

typedef struct {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    VariableItemList* settings_view;
    Widget* about_widget;

    /* Services */
    NotificationApp* notifications;

    /* Configuration — edited via Settings view, read by worker. */
    RhConfig config;

    /* UART layer */
    RhUart* uart;

    /* Poll timer — fires every config.poll_interval_s seconds. */
    FuriTimer* poll_timer;

    /* Mutex protecting config (read by timer cb, written by settings). */
    FuriMutex* config_mutex;

    /* Active view tracker for navigation_callback */
    RhView current_view;

    /* Monotonically increasing threat for notification de-duplication.
   * Set to RhThreatNone so the first High always fires a notification. */
    RhThreatLevel last_notified_threat;
} RhApp;

/* =========================================================================
 * Entry point (rayhunter.c)
 * ========================================================================= */

int32_t rayhunter_client_app(void* p);
