#pragma once
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "alerts.h"
#include "link.h"
#include "settings.h"
#include "transport.h"
#include "views/dashboard_view.h"
#include "views/alert_view.h"

#define PHR_ALERT_QUEUE 4

typedef enum {
    PhrViewDashboard,
    PhrViewAlert,
    PhrViewList,
} PhrViewId;

typedef enum {
    PhrEventOpenSettings = 100,
    PhrEventAckAlert,
} PhrCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Storage* storage;

    PhrDashboardView* dashboard;
    PhrAlertView* alert_view;
    VariableItemList* list;

    PhrSettings settings;
    AlertEngine alerts;

    PhrLink* link;
    const PhrTransportApi* transport_api;
    PhrTransport* transport;
    bool transport_failed;
    bool start_pending; // transport is started on the first tick
    bool backlight_forced;

    // Latest snapshot (updated by phr_app_poll on the UI thread)
    PhrLinkSnapshot snap;
    uint32_t last_alert_eval_s;

    // Pending alerts to show on the alert screen
    AlertEvent pending[PHR_ALERT_QUEUE];
    size_t pending_count;
    AlertEvent shown;

    // scene scratch
    uint8_t editing_rule;
    char rule_txt[AlertRuleCount][14];
    char edit_txt[12];
} PhrApp;

/** Starts the transport selected in settings (stops the previous one first). */
void phr_app_apply_transport(PhrApp* app);
/** Periodic work: telemetry snapshot, HELLO, alert engine, signals. UI thread. */
void phr_app_poll(PhrApp* app);
uint32_t phr_app_now_s(void);
bool phr_app_pop_pending(PhrApp* app, AlertEvent* out);
