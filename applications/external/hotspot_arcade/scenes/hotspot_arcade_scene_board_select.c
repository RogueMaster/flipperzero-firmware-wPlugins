#include "../hotspot_arcade_i.h"

// Pick which ESP board to flash, then hand off to the flasher. Each supported board's
// firmware is bundled in the fap (assets/firmware/<board>/), so this is a pure selector
// with no file picker. Adding a board is one more submenu row + one asset folder.
//
// The scene is transient: when the flasher returns here (previous_scene) it pops
// straight back to whoever opened the picker (the menu, or the lobby's no-board
// prompt), so flashing still feels like a single step. Scene state 1 marks "sent to
// the flasher"; on the return re-enter we pop instead of rebuilding the list.
// Each board gets two rows: one that pulses DTR/RTS to enter download mode on
// its own, and one for boards wired differently, where the user still holds
// BOOT and taps RESET by hand.
typedef enum {
    BoardOfficialBoot,
    BoardOfficial,
    BoardWroomBoot,
    BoardWroom,
    BoardC5Boot,
    BoardC5,
} BoardIndex;

static void ha_board_cb(void* context, uint32_t index) {
    HotspotArcadeApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hotspot_arcade_scene_board_select_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    if(scene_manager_get_scene_state(app->scene_manager, HaSceneBoardSelect) == 1) {
        // Returning from the flasher: pass straight back to the caller.
        scene_manager_set_scene_state(app->scene_manager, HaSceneBoardSelect, 0);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select your board");
    submenu_add_item(app->submenu, "Dev Board (auto boot)", BoardOfficialBoot, ha_board_cb, app);
    submenu_add_item(app->submenu, "Official Dev Board", BoardOfficial, ha_board_cb, app);
    submenu_add_item(app->submenu, "WROOM (auto boot)", BoardWroomBoot, ha_board_cb, app);
    submenu_add_item(app->submenu, "ESP32 WROOM", BoardWroom, ha_board_cb, app);
    submenu_add_item(app->submenu, "C5 (auto boot)", BoardC5Boot, ha_board_cb, app);
    submenu_add_item(app->submenu, "ESP32 C5", BoardC5, ha_board_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
}

bool hotspot_arcade_scene_board_select_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    const char* manifest;
    bool auto_boot = false;
    switch(event.event) {
    case BoardOfficialBoot:
        auto_boot = true;
        /* fallthrough */
    case BoardOfficial:
        manifest = HA_OFFICIAL_FW;
        break;
    case BoardWroomBoot:
        auto_boot = true;
        /* fallthrough */
    case BoardWroom:
        manifest = HA_WROOM_FW;
        break;
    case BoardC5Boot:
        auto_boot = true;
        /* fallthrough */
    case BoardC5:
        manifest = HA_C5_FW;
        break;
    default:
        return false;
    }
    furi_string_set(app->flash_manifest, manifest);
    app->flash_auto_boot = auto_boot;
    scene_manager_set_scene_state(app->scene_manager, HaSceneBoardSelect, 1);
    scene_manager_next_scene(app->scene_manager, HaSceneFlasher);
    return true;
}

void hotspot_arcade_scene_board_select_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    submenu_reset(app->submenu);
}
