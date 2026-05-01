#include "prefs.h"
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
    state->suppress_speed_chime     = g_prefs.getBool("chime",  true);
    state->force_fsd                = g_prefs.getBool("force",  false);
    state->tlssc_restore            = g_prefs.getBool("tlssc",  false);
    state->precondition             = g_prefs.getBool("precond",false);
    state->emergency_vehicle_detect = g_prefs.getBool("emrg",   false);
    state->bms_output               = g_prefs.getBool("bms",    false);
    state->sleep_idle_ms            = g_prefs.getUInt("sleep",  SLEEP_IDLE_MS);

    // WiFi
    if (g_prefs.isKey("wss")) g_prefs.getString("wss").toCharArray(state->wifi_ssid, sizeof(state->wifi_ssid));
    if (g_prefs.isKey("wsp")) g_prefs.getString("wsp").toCharArray(state->wifi_pass, sizeof(state->wifi_pass));
    state->wifi_hidden = g_prefs.getBool("wsh", false);

    state->op_mode = (OpMode)g_prefs.getUChar("mode", (uint8_t)OpMode_ListenOnly);
    
    Serial.printf("[NVS] Loaded: NAG=%d Sleep=%u SSID=\"%s\" HIDDEN=%d\n",
                  state->nag_killer, state->sleep_idle_ms, state->wifi_ssid, state->wifi_hidden);
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
    g_prefs.putBool("chime",  state->suppress_speed_chime);
    g_prefs.putBool("force",  state->force_fsd);
    g_prefs.putBool("tlssc",  state->tlssc_restore);
    g_prefs.putBool("precond",state->precondition);
    g_prefs.putBool("emrg",   state->emergency_vehicle_detect);
    g_prefs.putBool("bms",    state->bms_output);
    g_prefs.putUInt("sleep",  state->sleep_idle_ms);

    // WiFi
    g_prefs.putString("wss",  state->wifi_ssid);
    g_prefs.putString("wsp",  state->wifi_pass);
    g_prefs.putBool("wsh",    state->wifi_hidden);

    g_prefs.putUChar("mode",  (uint8_t)state->op_mode);
    
    Serial.printf("[NVS] Saved: NAG=%d Sleep=%u SSID=\"%s\" HIDDEN=%d\n",
                  state->nag_killer, state->sleep_idle_ms, state->wifi_ssid, state->wifi_hidden);
    g_prefs.end();
}
