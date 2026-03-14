#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <furi_hal_infrared.h>

/* ─────────────────────────────────────────────────────
 * Scene IDs — order MUST match skidcity_scene_config.h
 * ───────────────────────────────────────────────────── */
typedef enum {
    SkidCitySceneMainMenu = 0,
    SkidCitySceneFeatureMenu,
    SkidCitySceneTrafficDemo,
    SkidCitySceneGenericDemo,
    SkidCitySceneFeatureAbout,
    SkidCitySceneBanned,
    SkidCitySceneCfaaInfo,
    SkidCitySceneAppAbout,
    SkidCitySceneCount,
} SkidCityScene;

/* ─────────────────────────────────────────────────────
 * View IDs registered with ViewDispatcher
 * ───────────────────────────────────────────────────── */
typedef enum {
    SkidCityViewSubmenu = 0,
    SkidCityViewWidget,
    SkidCityViewTrafficDemo,
    SkidCityViewBanned,
    SkidCityViewIrDemo,
} SkidCityViewId;

/* ─────────────────────────────────────────────────────
 * Feature IDs — one per "skid thing" on main menu
 * ───────────────────────────────────────────────────── */
typedef enum {
    SkidCityFeatureTraffic = 0,
    SkidCityFeatureWifi,
    SkidCityFeatureCar,
    SkidCityFeatureCards,
    SkidCityFeatureDoors,
    SkidCityFeatureTv,
    SkidCityFeatureAirplane,
    SkidCityFeatureJammer,
    SkidCityFeatureAtm,
    SkidCityFeatureRfJam,
    SkidCityFeatureBleSpam,
    SkidCityFeatureIrAbuse,
    SkidCityFeatureCount,
} SkidCityFeature;

/* What "Try It" does for each feature */
typedef enum {
    SkidCityDemoTrafficLed = 0,
    SkidCityDemoIrBlink,
    SkidCityDemoBanned,
    SkidCityDemoSkid,
    SkidCityDemoFcc,
} SkidCityDemoType;

/* Which banned-screen variant to display */
typedef enum {
    SkidCityBannedTypeCFAA = 0,
    SkidCityBannedTypeSkid,
    SkidCityBannedTypeFCC,
} SkidCityBannedVariant;

/* Per-feature data record */
typedef struct {
    const char*      menu_label;
    const char*      submenu_header;
    const char*      demo_item;
    const char*      about_label;  /* sub-menu item text for "about" */
    const char*      about_title;
    const char*      about_body;
    SkidCityDemoType demo_type;
} SkidCityFeatureInfo;

extern const SkidCityFeatureInfo skidcity_features[SkidCityFeatureCount];

/* View models */
typedef struct {
    uint8_t state; /* 0=red 1=yellow 2=green */
} TrafficModel;

typedef struct {
    SkidCityBannedVariant variant;
} BannedModel;

typedef struct {
    bool transmitting;
} IrDemoModel;

/* Application context */
typedef struct {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;
    Submenu*         submenu;
    Widget*          widget;
    NotificationApp* notifications;
    View*            traffic_view;
    View*            banned_view;
    View*            ir_view;
    SkidCityFeature  current_feature;
    FuriTimer*       header_timer;
    uint8_t          header_offset;
    char             header_buf[32];
    FuriTimer*       ir_blink_timer;
    bool             ir_tx_active;
    uint32_t         main_menu_selected_index;
    uint32_t         feature_menu_selected_index;
} SkidCityApp;
