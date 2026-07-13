#include "prefs.h"
#include "blackbox.h"   // BLACKBOX_DEFAULT_ENABLED
#include <Preferences.h>

static Preferences g_prefs;
static const char *NS = "fsd";

void prefs_load(FSDState *state) {
    g_prefs.begin(NS, /*readOnly=*/true);
    if (!g_prefs.isKey("ok")) {
        Serial.println("[NVS] No saved settings found (first boot)");
        g_prefs.end();
        return;
    }
    state->nag_killer               = g_prefs.getBool("nag",    true);
    state->continuous_ap            = g_prefs.getBool("contap", false);
    state->ap_first                 = g_prefs.getBool("apfirst",false);
    state->ap_first_edge            = g_prefs.getBool("apfe",   false);
    state->nag_epas_faithful        = g_prefs.getBool("nagf",   false);
    state->soft_engage              = g_prefs.getBool("soft",   false);
    state->nag_burst                = g_prefs.getBool("nagb",   false);
    state->abort_guard              = g_prefs.getBool("abortg", false);
    state->suppress_speed_chime     = g_prefs.getBool("chime",  true);
    state->ignore_ota               = g_prefs.getBool("ignota", false);
    state->fsd_unlock               = g_prefs.getBool("unlock", false);
    state->force_fsd                = g_prefs.getBool("force",  false);
    state->china_mode               = g_prefs.getBool("china",  false);
    state->tlssc_restore            = g_prefs.getBool("tlssc",  false);
    state->precondition             = g_prefs.getBool("precond",false);
    state->emergency_vehicle_detect = g_prefs.getBool("emrg",   false);
    state->bms_output               = g_prefs.getBool("bms",    false);
    state->firmware_14x_warning     = g_prefs.getBool("14x",    true);
    state->blackbox_enabled         = g_prefs.getBool("bbx",    BLACKBOX_DEFAULT_ENABLED);
#if defined(BOARD_TTGO_DISPLAY)
    state->display_enabled          = g_prefs.getBool("disp",   true);
    state->display_brightness       = g_prefs.getUChar("disp_br", 50);
    state->display_timeout_s        = g_prefs.getUInt("disp_to",  60);
#endif
    state->sleep_idle_ms            = g_prefs.getUInt("sleep",  SLEEP_IDLE_MS);

    // WiFi
    if (g_prefs.isKey("wss")) g_prefs.getString("wss").toCharArray(state->wifi_ssid, sizeof(state->wifi_ssid));
    if (g_prefs.isKey("wsp")) g_prefs.getString("wsp").toCharArray(state->wifi_pass, sizeof(state->wifi_pass));
    state->wifi_hidden = g_prefs.getBool("wsh", false);
    if (g_prefs.isKey("stas")) g_prefs.getString("stas").toCharArray(state->wifi_sta_ssid, sizeof(state->wifi_sta_ssid));
    if (g_prefs.isKey("stap")) g_prefs.getString("stap").toCharArray(state->wifi_sta_pass, sizeof(state->wifi_sta_pass));

    state->op_mode = (OpMode)g_prefs.getUChar("mode", (uint8_t)OpMode_ListenOnly);

    // Configurable nag-context signal mapping (#122)
    state->cfg_das_id        = g_prefs.getUShort("cdid",  0);
    state->cfg_apstate_byte  = g_prefs.getUChar("capb",   0);
    state->cfg_apstate_shift = g_prefs.getUChar("caps",   0);
    state->cfg_apstate_mask  = g_prefs.getUChar("capm",   0x0F);
    state->cfg_handson_byte  = g_prefs.getUChar("chob",   0);
    state->cfg_handson_shift = g_prefs.getUChar("chos",   0);
    state->cfg_handson_mask  = g_prefs.getUChar("chom",   0x0F);
    state->cfg_steer_id      = g_prefs.getUShort("csid",  0);
    state->cfg_steer_hi      = g_prefs.getUChar("cshi",   1);
    state->cfg_steer_lo      = g_prefs.getUChar("cslo",   0);

    Serial.printf("[NVS] Loaded: FSDUnlock=%d NAG=%d ContinuousAP=%d IgnoreOTA=%d China=%d Chime=%d Sleep=%u AP=\"%s\" STA=\"%s\" HIDDEN=%d\n",
                  state->fsd_unlock, state->nag_killer, state->continuous_ap, state->ignore_ota,
                  state->china_mode, state->suppress_speed_chime,
                  state->sleep_idle_ms, state->wifi_ssid, state->wifi_sta_ssid,
                  state->wifi_hidden);
    g_prefs.end();
}

void prefs_clear() {
    g_prefs.begin(NS, /*readOnly=*/false);
    g_prefs.clear();
    g_prefs.end();
    Serial.println("[NVS] All settings erased — factory reset");
}

void prefs_save(const FSDState *state) {
    g_prefs.begin(NS, /*readOnly=*/false);
    g_prefs.putBool("ok",     true);
    g_prefs.putBool("nag",    state->nag_killer);
    g_prefs.putBool("contap", state->continuous_ap);
    g_prefs.putBool("apfirst",state->ap_first);
    g_prefs.putBool("apfe",   state->ap_first_edge);
    g_prefs.putBool("nagf",   state->nag_epas_faithful);
    g_prefs.putBool("soft",   state->soft_engage);
    g_prefs.putBool("nagb",   state->nag_burst);
    g_prefs.putBool("abortg", state->abort_guard);
    g_prefs.putBool("chime",  state->suppress_speed_chime);
    g_prefs.putBool("ignota", state->ignore_ota);
    g_prefs.putBool("unlock", state->fsd_unlock);
    g_prefs.putBool("force",  state->force_fsd);
    g_prefs.putBool("china",  state->china_mode);
    g_prefs.putBool("tlssc",  state->tlssc_restore);
    g_prefs.putBool("precond",state->precondition);
    g_prefs.putBool("emrg",   state->emergency_vehicle_detect);
    g_prefs.putBool("bms",    state->bms_output);
    g_prefs.putBool("14x",    state->firmware_14x_warning);
    g_prefs.putBool("bbx",    state->blackbox_enabled);
#if defined(BOARD_TTGO_DISPLAY)
    g_prefs.putBool("disp",   state->display_enabled);
    g_prefs.putUChar("disp_br", state->display_brightness);
    g_prefs.putUInt("disp_to",  state->display_timeout_s);
#endif
    g_prefs.putUInt("sleep",  state->sleep_idle_ms);

    // WiFi
    g_prefs.putString("wss",  state->wifi_ssid);
    g_prefs.putString("wsp",  state->wifi_pass);
    g_prefs.putBool("wsh",    state->wifi_hidden);
    g_prefs.putString("stas", state->wifi_sta_ssid);
    g_prefs.putString("stap", state->wifi_sta_pass);

    g_prefs.putUChar("mode",  (uint8_t)state->op_mode);

    // Configurable nag-context signal mapping (#122)
    g_prefs.putUShort("cdid", state->cfg_das_id);
    g_prefs.putUChar("capb",  state->cfg_apstate_byte);
    g_prefs.putUChar("caps",  state->cfg_apstate_shift);
    g_prefs.putUChar("capm",  state->cfg_apstate_mask);
    g_prefs.putUChar("chob",  state->cfg_handson_byte);
    g_prefs.putUChar("chos",  state->cfg_handson_shift);
    g_prefs.putUChar("chom",  state->cfg_handson_mask);
    g_prefs.putUShort("csid", state->cfg_steer_id);
    g_prefs.putUChar("cshi",  state->cfg_steer_hi);
    g_prefs.putUChar("cslo",  state->cfg_steer_lo);

    Serial.printf("[NVS] Saved: FSDUnlock=%d NAG=%d ContinuousAP=%d IgnoreOTA=%d China=%d Chime=%d Sleep=%u AP=\"%s\" STA=\"%s\" HIDDEN=%d\n",
                  state->fsd_unlock, state->nag_killer, state->continuous_ap, state->ignore_ota,
                  state->china_mode, state->suppress_speed_chime,
                  state->sleep_idle_ms, state->wifi_ssid, state->wifi_sta_ssid,
                  state->wifi_hidden);
    g_prefs.end();
}
