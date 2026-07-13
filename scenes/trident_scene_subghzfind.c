#include "../trident_i.h"
#include <stdio.h>

/*
 * CC1101 Frequency Finder: camp one frequency and watch the signal meter.
 *   Left / Right  - tune down / up by the current step
 *   Up   / Down   - larger / smaller step (10 kHz .. 10 MHz)
 *   OK            - reset the peak hold
 * Signal drives Geiger-style clicks when Sound is on.
 */

#define FIND_FREQ_MIN 287000000u
#define FIND_FREQ_MAX 928000000u

static const uint32_t find_steps[] = {10000u, 100000u, 1000000u, 10000000u};
#define FIND_STEP_COUNT 4

static const char* find_step_label(uint8_t i) {
    switch(i % FIND_STEP_COUNT) {
    case 0:
        return "10k";
    case 1:
        return "100k";
    case 2:
        return "1M";
    default:
        return "10M";
    }
}

static void trident_scene_subghzfind_input(void* context, InputKey key) {
    TridentApp* app = context;
    uint8_t step_i = (uint8_t)scene_manager_get_scene_state(app->scene_manager, TridentSceneSubghzfind);
    uint32_t step = find_steps[step_i % FIND_STEP_COUNT];
    uint32_t f = subghz_radio_get_camp_freq(app->subghz);

    switch(key) {
    case InputKeyLeft:
        f = (f > FIND_FREQ_MIN + step) ? f - step : FIND_FREQ_MIN;
        subghz_radio_set_camp_freq(app->subghz, f);
        break;
    case InputKeyRight:
        f = (f + step < FIND_FREQ_MAX) ? f + step : FIND_FREQ_MAX;
        subghz_radio_set_camp_freq(app->subghz, f);
        break;
    case InputKeyUp:
        step_i = (uint8_t)((step_i + 1) % FIND_STEP_COUNT);
        scene_manager_set_scene_state(app->scene_manager, TridentSceneSubghzfind, step_i);
        break;
    case InputKeyDown:
        step_i = (uint8_t)((step_i + FIND_STEP_COUNT - 1) % FIND_STEP_COUNT);
        scene_manager_set_scene_state(app->scene_manager, TridentSceneSubghzfind, step_i);
        break;
    case InputKeyOk:
        subghz_radio_reset(app->subghz);
        break;
    default:
        break;
    }
}

void trident_scene_subghzfind_on_enter(void* context) {
    TridentApp* app = context;

    trident_link_disarm(app);
    meter_view_set_input_callback(app->meter_view, trident_scene_subghzfind_input, app);

    MeterSnapshot boot;
    memset(&boot, 0, sizeof(boot));
    boot.present = true;
    boot.running = true;
    strncpy(boot.title, "CC1101 Finder", sizeof(boot.title) - 1);
    strncpy(boot.value, "--", sizeof(boot.value) - 1);
    strncpy(boot.unit, "dBm", sizeof(boot.unit) - 1);
    strncpy(boot.sub, "tuning...", sizeof(boot.sub) - 1);
    meter_view_set_snapshot(app->meter_view, &boot);

    subghz_radio_configure(
        app->subghz, 0, app->settings.cc1101_device == TridentCc1101External);
    subghz_radio_set_mode(app->subghz, SubghzModeCamp);
    subghz_radio_start(app->subghz);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewMeter);
}

bool trident_scene_subghzfind_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        MeterSnapshot snap;
        subghz_radio_get_meter(app->subghz, &snap);

        uint8_t step_i =
            (uint8_t)scene_manager_get_scene_state(app->scene_manager, TridentSceneSubghzfind);
        snprintf(snap.foot, sizeof(snap.foot), "<>tune  ^v %s  OK0", find_step_label(step_i));
        meter_view_set_snapshot(app->meter_view, &snap);

        // Geiger feedback: click rate rises with signal strength.
        if(snap.level > 25) {
            static uint8_t t = 0;
            uint8_t period = (uint8_t)((110 - snap.level) / 12);
            if(period < 1) period = 1;
            if((t++ % period) == 0) trident_click(app);
        }
        consumed = true;
    }
    return consumed;
}

void trident_scene_subghzfind_on_exit(void* context) {
    TridentApp* app = context;
    subghz_radio_stop(app->subghz);
}
