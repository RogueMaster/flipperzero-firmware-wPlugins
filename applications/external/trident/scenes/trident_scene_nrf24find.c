#include "../trident_i.h"
#include <stdio.h>

/*
 * NRF24 Channel Finder: camp one of the 126 channels and watch the RPD
 * hit-rate meter.
 *   Left / Right - previous / next channel
 *   Up   / Down  - +10 / -10 channels
 *   OK           - reset the peak hold
 */
#define NRF_CH_MAX 125

static void trident_scene_nrf24find_set(TridentApp* app, int ch) {
    if(ch < 0) ch = 0;
    if(ch > NRF_CH_MAX) ch = NRF_CH_MAX;
    nrf24_radio_set_channel(app->nrf24, (uint8_t)ch);
}

static void trident_scene_nrf24find_input(void* context, InputKey key) {
    TridentApp* app = context;
    int ch = nrf24_radio_get_channel(app->nrf24);
    switch(key) {
    case InputKeyLeft:
        trident_scene_nrf24find_set(app, ch - 1);
        break;
    case InputKeyRight:
        trident_scene_nrf24find_set(app, ch + 1);
        break;
    case InputKeyDown:
        trident_scene_nrf24find_set(app, ch - 10);
        break;
    case InputKeyUp:
        trident_scene_nrf24find_set(app, ch + 10);
        break;
    case InputKeyOk:
        nrf24_radio_reset(app->nrf24);
        break;
    default:
        break;
    }
}

void trident_scene_nrf24find_on_enter(void* context) {
    TridentApp* app = context;

    trident_link_disarm(app);
    meter_view_set_input_callback(app->meter_view, trident_scene_nrf24find_input, app);

    MeterSnapshot boot;
    memset(&boot, 0, sizeof(boot));
    boot.present = true;
    boot.running = true;
    strncpy(boot.title, "NRF24 Finder", sizeof(boot.title) - 1);
    strncpy(boot.value, "--", sizeof(boot.value) - 1);
    strncpy(boot.unit, "%", sizeof(boot.unit) - 1);
    strncpy(boot.sub, "tuning...", sizeof(boot.sub) - 1);
    meter_view_set_snapshot(app->meter_view, &boot);

    nrf24_radio_set_mode(app->nrf24, Nrf24ModeCamp);
    nrf24_radio_start(app->nrf24);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewMeter);
}

bool trident_scene_nrf24find_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        MeterSnapshot snap;
        nrf24_radio_get_meter(app->nrf24, &snap);
        strncpy(snap.foot, "<>chan  ^v +-10  OK zero", sizeof(snap.foot) - 1);
        meter_view_set_snapshot(app->meter_view, &snap);

        if(snap.level > 20) {
            static uint8_t t = 0;
            uint8_t period = (uint8_t)((110 - snap.level) / 12);
            if(period < 1) period = 1;
            if((t++ % period) == 0) trident_click(app);
        }
        consumed = true;
    }
    return consumed;
}

void trident_scene_nrf24find_on_exit(void* context) {
    TridentApp* app = context;
    nrf24_radio_stop(app->nrf24);
}
