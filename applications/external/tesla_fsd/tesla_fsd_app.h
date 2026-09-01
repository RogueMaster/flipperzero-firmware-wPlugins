#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>

#include "libraries/mcp_can_2515.h"
#include "fsd_logic/fsd_handler.h"
#include "fsd_logic/fsd_profile.h"

#define FSD_SEND_MAX_STEPS 48

#define TESLA_FSD_VERSION "2.16.1"

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

    Storage* storage;
    DialogsApp* dialogs;

    // Loaded .cantest SEND profile (user-authored test packets)
    FsdProfileStep send_steps[FSD_SEND_MAX_STEPS];
    uint8_t send_step_count;
    uint8_t send_blocked; // count of safety-denied lines skipped at load (e.g. 0x229)
    char send_name[34];
    bool send_armed; // set by the ARM button; worker sends when the interlock allows
    uint32_t send_sent; // frames actually transmitted (for display/result)

    FuriThread* worker_thread;
    FuriMutex* mutex;

    TeslaHWVersion hw_version;
    FSDState fsd_state;

    // feature toggles (set in settings, copied to fsd_state at start)
    bool force_fsd;
    bool suppress_speed_chime;
    bool emergency_vehicle_detect;
    bool nag_killer;
    bool nag_burst; // burst/pause echo (~1s on / ~1.5s off), #122
    bool precondition; // periodic 0x082 inject for battery preheat
    OpMode op_mode; // Active / ListenOnly / Service
    uint8_t mcp_clock; // 0 = 16MHz (default), 1 = 8MHz
    uint8_t signal_map; // DAS AP/hands-on read location preset (see signal_map_apply)
    bool gtw_shield; // 0x7FF GTW Config Replay — replay learned-healthy frames
    bool tlssc_restore; // 0x331 DAS config spoof to restore TLSSC
    bool ap_first; // 2026.14.x: delay injection until AP is engaged
    bool ap_first_edge; // experimental Instant Engage: inject at AP-engage onset, skip the 1s debounce (#129/#108)
    bool ap_first_minimal; // experimental Minimal Inject: brief AP-enable burst at engage then stop, off the abort edge (#108/#129)
    bool soft_engage; // steer-jerk: hold activation until wheel centred (#108)
    bool nag_epas_faithful; // 2026.14.x experimental: mirror the in-the-wild 0x370 scheme (no handsOnLevel flip; torque centred at 0 Nm with live variance)
    bool firmware_14x_warning; // 2026.14.x: show TX-disables-AP warning in running scene (default ON, opt-out for pre-14.x users)
    bool gtw_tier_override; // 0x7FF active tier=SELF_DRIVING override
    bool scroll_press_ap; // 0x3C2 scroll-press AP engage (HW4-only, Service mode)

    // CAN capture: log every RX frame to SD in candump-ASCII (feeds the cracker)
    bool can_capture;
    uint32_t capture_count; // worker-updated count of frames written this run

    // driver assist overrides (0x3F8 + 0x3FD)
    bool assist_nav_enable; // nav-based FSD routing (EU/restricted)
    bool assist_hands_off; // UI-level hands-on disable
    bool assist_dev_mode; // developer mode flag
    bool assist_lhd_override; // force left-hand drive
    bool assist_show_lane_graph; // lane visualization
    bool assist_tlssc_bit38; // explicit TLSSC enable on 0x3FD mux0
    bool assist_telemetry_off; // experimental: clear reachable telemetry flags (0x3F8 19/42/43/44/55 + 0x3FD mux1 48/50)

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

// Signal Map presets — where the nag/AP logic reads DAS_autopilotState and
// DAS_handsOnState. Some cars carry the live state in a non-standard byte of
// 0x39B/0x399, so let the owner relocate the read without a firmware fallback.
// The configurable-mapping mechanism itself lives in the shared core
// (cfg_das_id + fsd_apply_signal_config); this only picks a position.
//   0 Auto           — cfg_das_id = 0, auto-detect (default)
//   1 0x39B b0 (HW4) — live DAS state in byte0 low nibble (Juniper-style)
//   2 0x39B b1 (HW4) — standard HW4 byte1 high nibble
//   3 0x399 b0 (HW3) — HW3 / Legacy byte0 low nibble
#define SIGNAL_MAP_COUNT 4

static inline void signal_map_apply(FSDState* state, uint8_t idx) {
    // handsOnState is byte5[5:2] low nibble on both 0x39B and 0x399.
    state->cfg_handson_byte = 5;
    state->cfg_handson_shift = 2;
    state->cfg_handson_mask = 0x0F;
    switch(idx) {
    case 1: // 0x39B byte0 low nibble (live state on some HW4 cars)
        state->cfg_das_id = 0x39B;
        state->cfg_apstate_byte = 0;
        state->cfg_apstate_shift = 0;
        state->cfg_apstate_mask = 0x0F;
        break;
    case 2: // 0x39B byte1 high nibble (standard HW4)
        state->cfg_das_id = 0x39B;
        state->cfg_apstate_byte = 1;
        state->cfg_apstate_shift = 4;
        state->cfg_apstate_mask = 0x0F;
        break;
    case 3: // 0x399 byte0 low nibble (HW3 / Legacy)
        state->cfg_das_id = 0x399;
        state->cfg_apstate_byte = 0;
        state->cfg_apstate_shift = 0;
        state->cfg_apstate_mask = 0x0F;
        break;
    default: // 0 = Auto: cfg_das_id == 0 disables the override
        state->cfg_das_id = 0;
        state->cfg_apstate_byte = 0;
        state->cfg_apstate_shift = 0;
        state->cfg_apstate_mask = 0x0F;
        state->cfg_handson_byte = 0;
        state->cfg_handson_shift = 0;
        state->cfg_handson_mask = 0x0F;
        break;
    }
    // cfg_steer_* stay on auto (untouched) for every preset.
}
