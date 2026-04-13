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

#define TESLA_FSD_VERSION FAP_VERSION

typedef enum {
    TeslaFSDSceneMainMenu,
    TeslaFSDSceneSettings,
    TeslaFSDSceneExtras,
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
    bool nag_killer;
    bool precondition; // periodic 0x082 inject for battery preheat
    OpMode op_mode; // Active / ListenOnly / Service
    uint8_t mcp_clock; // 0 = 16MHz (default), 1 = 8MHz

    // extras toggles (BETA — need on-vehicle verification per CAN ID)
    bool extra_hazard_lights;
    bool extra_rear_window_heat;
    bool extra_auto_wipers_off;
    bool extra_fold_mirrors;
    bool extra_rear_fog;
    uint8_t extra_steering_mode; // 0=no change, 1=comfort, 2=standard, 3=sport (Chassis CAN)
    bool extra_highbeam_strobe; // rapid high beam flash (Party CAN 0x249)
    bool extra_turn_left; // inject left turn signal
    bool extra_turn_right; // inject right turn signal
} TeslaFSDApp;

TeslaFSDApp* tesla_fsd_app_alloc(void);
void tesla_fsd_app_free(TeslaFSDApp* app);
int32_t tesla_fsd_main(void* p);
