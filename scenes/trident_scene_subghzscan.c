#include "../trident_i.h"

// OK on the analyzer clears the max-hold.
static void trident_scene_subghzscan_ok_cb(void* context) {
    TridentApp* app = context;
    subghz_radio_reset(app->subghz);
}

void trident_scene_subghzscan_on_enter(void* context) {
    TridentApp* app = context;

    trident_link_disarm(app);

    spectrum_view_set_ok_callback(app->spectrum_view, trident_scene_subghzscan_ok_cb, app);

    SpectrumSnapshot boot;
    memset(&boot, 0, sizeof(boot));
    boot.present = true;
    boot.running = true;
    strncpy(boot.title, "CC1101 Sub-GHz", sizeof(boot.title) - 1);
    strncpy(boot.peak_label, "starting...", sizeof(boot.peak_label) - 1);
    const char* lo = trident_subghz_bands[app->settings.subghz_band % TRIDENT_SUBGHZ_BAND_COUNT].lo_label;
    const char* hi = trident_subghz_bands[app->settings.subghz_band % TRIDENT_SUBGHZ_BAND_COUNT].hi_label;
    strncpy(boot.lo_label, lo, sizeof(boot.lo_label) - 1);
    strncpy(boot.hi_label, hi, sizeof(boot.hi_label) - 1);
    spectrum_view_set_snapshot(app->spectrum_view, &boot);

    subghz_radio_configure(
        app->subghz,
        app->settings.subghz_band,
        app->settings.cc1101_device == TridentCc1101External);
    subghz_radio_set_mode(app->subghz, SubghzModeSweep);
    subghz_radio_start(app->subghz);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSpectrum);
}

bool trident_scene_subghzscan_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        SpectrumSnapshot snap;
        subghz_radio_get_snapshot(app->subghz, &snap);
        spectrum_view_set_snapshot(app->spectrum_view, &snap);
        consumed = true;
    }
    return consumed;
}

void trident_scene_subghzscan_on_exit(void* context) {
    TridentApp* app = context;
    subghz_radio_stop(app->subghz);
}
