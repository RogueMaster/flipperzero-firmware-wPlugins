#pragma once

#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification.h>
#include <storage/storage.h>

#include "uart/uhf_u107.h"

// ids for all scenes used by the app
typedef enum {
    OpenDisplay,
    ScanDisplay,
    ParseDisplay,
    RawDataDisplay,
    ui_count
} UIScene;

// ids for the type of view used by the app
typedef enum {
    View_OpenDisplay,
    View_ScanDisplay,
    View_ParseDisplay,
    View_RawDataDisplay,
    view_count
} UIView;

// States for the scanning thread
typedef enum {
    ScanThreadIdle,
    ScanThreadRunning,
    ScanThreadStopping
} ScanThreadState;

// Defines the data for use within each scene
typedef struct {
    Widget* open_widget;
} OpenScene;

typedef struct {
    Widget* scan_widget;
    
    FuriThread* scan_thread;
    ScanThreadState thread_state;
} ScanScene;

typedef struct {
    Widget* parse_widget;
} ParseScene;

typedef struct {
    Widget* raw_data_widget;
} RawDataScene;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Gui* gui;
    
    UhfDevice* uhf_device;
    UhfEpcTagData* epc_data;
    UhfUserMemoryData* user_mem_data;
    
    // The scenes that will be displayed by the manager
    OpenScene* open_scene;
    ScanScene* scan_scene;
    ParseScene* parse_scene;
    RawDataScene* raw_data_scene;
} UI;

UI* ui_alloc();
void ui_start(UI* ui);
void ui_free(UI* ui);

OpenScene* open_scene_alloc();
void open_scene_on_enter(void* context);
bool open_scene_on_event(void* context, SceneManagerEvent event);
void open_scene_on_exit(void* context);
void open_scene_free(OpenScene* open_scene);

ScanScene* scan_scene_alloc(UI* context);
void scan_scene_on_enter(void* context);
bool scan_scene_on_event(void* context, SceneManagerEvent event);
void scan_scene_on_exit(void* context);
void scan_scene_free(ScanScene* scan_scene);

ParseScene* parse_scene_alloc();
void parse_scene_on_enter(void* context);
bool parse_scene_on_event(void* context, SceneManagerEvent event);
void parse_scene_on_exit(void* context);
void parse_scene_free(ParseScene* parse_scene);

RawDataScene* raw_data_scene_alloc();
void raw_data_scene_on_enter(void* context);
bool raw_data_scene_on_event(void* context, SceneManagerEvent event);
void raw_data_scene_on_exit(void* context);
void raw_data_scene_free(RawDataScene* raw_data_scene);
