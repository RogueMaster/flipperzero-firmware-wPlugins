#include "../trident_i.h"
#include <stdio.h>

/*
 * CC1101 menu:
 *   - one band spectrum analyzer per band (300-348 / 387-464 / 779-928)
 *   - a frequency finder pre-tuned to each common preset
 * The active radio (internal / external CC1101) comes from Settings.
 */

#define SUBGHZ_PRESET_BASE TRIDENT_SUBGHZ_BAND_COUNT

static void trident_scene_subghz_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_subghz_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "CC1101 - Sub-GHz");
    for(uint8_t i = 0; i < TRIDENT_SUBGHZ_BAND_COUNT; i++) {
        char label[24];
        snprintf(label, sizeof(label), "Spectrum  %s", trident_subghz_band_label(i));
        submenu_add_item(menu, label, i, trident_scene_subghz_cb, app);
    }
    for(uint8_t i = 0; i < TRIDENT_SUBGHZ_PRESET_COUNT; i++) {
        char label[24];
        snprintf(label, sizeof(label), "Finder  %s MHz", trident_subghz_presets[i].label);
        submenu_add_item(menu, label, SUBGHZ_PRESET_BASE + i, trident_scene_subghz_cb, app);
    }

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneSubghz));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_subghz_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneSubghz, event.event);
        if(event.event < TRIDENT_SUBGHZ_BAND_COUNT) {
            app->settings.subghz_band = (uint8_t)event.event;
            scene_manager_next_scene(app->scene_manager, TridentSceneSubghzscan);
            consumed = true;
        } else if(event.event < SUBGHZ_PRESET_BASE + TRIDENT_SUBGHZ_PRESET_COUNT) {
            uint8_t p = (uint8_t)(event.event - SUBGHZ_PRESET_BASE);
            subghz_radio_set_camp_freq(app->subghz, trident_subghz_presets[p].hz);
            scene_manager_next_scene(app->scene_manager, TridentSceneSubghzfind);
            consumed = true;
        }
    }
    return consumed;
}

void trident_scene_subghz_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
