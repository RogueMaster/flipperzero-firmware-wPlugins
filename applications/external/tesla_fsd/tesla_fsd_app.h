#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>

#include "libraries/mcp_can_2515.h"
#include "fsd_logic/fsd_handler.h"

#define TESLA_FSD_VERSION "2.0.0"

typedef enum {
    TeslaFSDSceneMainMenu,
    TeslaFSDSceneSettings,
    TeslaFSDSceneHWDetect,
    TeslaFSDSceneHWSelect,
    TeslaFSDSceneRunning,
    TeslaFSDSceneAbout,
    TeslaFSDSceneCount,
} TeslaFSDScene;

typedef enum {
    TeslaFSDViewSubmenu,
    TeslaFSDViewWidget,
    TeslaFSDViewVarItemList,
} TeslaFSDView;

typedef enum {
    TeslaFSDEventHWDetected,
    TeslaFSDEventHWNotFound,
    TeslaFSDEventNoDevice,
    TeslaFSDEventSelectHW3,
    TeslaFSDEventSelectHW4,
} TeslaFSDEvent;

typedef enum {
    WorkerFlagStop = (1 << 0),
} WorkerFlag;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* var_item_list;

    MCP2515* mcp_can;
    CANFRAME can_frame;

    FuriThread* worker_thread;
    FuriMutex* mutex;

    TeslaHWVersion hw_version;
    FSDState fsd_state;

    // feature toggles (set in settings, copied to fsd_state at start)
    bool force_fsd;
    bool suppress_speed_chime;
    bool emergency_vehicle_detect;
} TeslaFSDApp;

TeslaFSDApp* tesla_fsd_app_alloc(void);
void tesla_fsd_app_free(TeslaFSDApp* app);
int32_t tesla_fsd_main(void* p);
