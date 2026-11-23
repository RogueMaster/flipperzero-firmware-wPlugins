#pragma once

#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "uart/tile_scanner_esp32.h"

#define MAX_RECENT_MACS 16
#define MAX_KNOWN_MACS  256

// ids for all scenes used by the app
typedef enum {
    ScanDisplay,
    IgnoredMacsDisplay,
    RecentMacsDisplay,
    ui_count
} UIScene;

// ids for the type of view used by the app
typedef enum {
    View_ScanDisplay,
    View_IgnoredMacsDisplay,
    View_RecentMacsDisplay,
    view_count
} UIView;

// Defines the data for use within each scene
typedef struct {
    Widget* scan_widget;
} ScanScene;

typedef struct {
    TextBox* ignored_macs_text_box;
    
    FuriString* display_text;
} IgnoredMacsScene;

typedef struct {
    TextBox* recent_macs_text_box;
    
    FuriString* display_text;
    TileScannerMacAddress recent_macs[MAX_RECENT_MACS];
} RecentMacsScene;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Gui* gui;
    
    TileScanner* tile_scanner;
    bool is_scanning;
    
    TileScannerMacAddress known_macs[MAX_KNOWN_MACS];
    uint16_t known_mac_index;
    
    // The scenes that will be displayed by the manager
    ScanScene* scan_scene;
    IgnoredMacsScene* ignored_macs_scene;
    RecentMacsScene* recent_macs_scene;
} UI;

UI* ui_alloc();
void ui_start(UI* ui);
void ui_free(UI* ui);

ScanScene* scan_scene_alloc();
void scan_scene_update();
void scan_scene_on_enter(void* context);
bool scan_scene_on_event(void* context, SceneManagerEvent event);
void scan_scene_on_exit(void* context);
void scan_scene_free(ScanScene* scan_scene);

IgnoredMacsScene* ignored_macs_scene_alloc();
void ignored_macs_scene_on_enter(void* context);
bool ignored_macs_scene_on_event(void* context, SceneManagerEvent event);
void ignored_macs_scene_on_exit(void* context);
void ignored_macs_scene_free(IgnoredMacsScene* ignored_macs_scene);

RecentMacsScene* recent_macs_scene_alloc();
void recent_macs_scene_on_enter(void* context);
bool recent_macs_scene_on_event(void* context, SceneManagerEvent event);
void recent_macs_scene_on_exit(void* context);
void recent_macs_handle_scanned_mac(TileScannerMacAddress* mac_address, UI* ui);
void recent_macs_scene_free(RecentMacsScene* recent_macs_scene);
