#include "../trident_i.h"

// OK on the analyzer clears the accumulated activity / peak hold.
static void trident_scene_nrf24scan_ok_cb(void* context) {
    TridentApp* app = context;
    nrf24_radio_reset(app->nrf24);
}

void trident_scene_nrf24scan_on_enter(void* context) {
    TridentApp* app = context;

    // Free the ESP32 UART just in case, then bring the 2.4 GHz analyzer up.
    trident_link_disarm(app);

    spectrum_view_set_ok_callback(app->spectrum_view, trident_scene_nrf24scan_ok_cb, app);

    SpectrumSnapshot boot;
    memset(&boot, 0, sizeof(boot));
    boot.present = true;
    boot.running = true;
    strncpy(boot.title, "NRF24 2.4GHz", sizeof(boot.title) - 1);
    strncpy(boot.peak_label, "starting...", sizeof(boot.peak_label) - 1);
    strncpy(boot.lo_label, "2400", sizeof(boot.lo_label) - 1);
    strncpy(boot.hi_label, "2525", sizeof(boot.hi_label) - 1);
    spectrum_view_set_snapshot(app->spectrum_view, &boot);

    nrf24_radio_start(app->nrf24);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSpectrum);
}

bool trident_scene_nrf24scan_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        SpectrumSnapshot snap;
        nrf24_radio_get_snapshot(app->nrf24, &snap);
        spectrum_view_set_snapshot(app->spectrum_view, &snap);
        consumed = true;
    }
    return consumed;
}

void trident_scene_nrf24scan_on_exit(void* context) {
    TridentApp* app = context;
    nrf24_radio_stop(app->nrf24);
}
