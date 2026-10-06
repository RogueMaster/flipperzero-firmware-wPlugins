#include "settings.h"
#include <flipper_format/flipper_format.h>

#define TAG              "PhrSettings"
#define SETTINGS_HEADER  "PC Health Remote settings"
#define SETTINGS_VERSION 1

static const char* const rule_keys[AlertRuleCount] = {
    [AlertCpuTemp] = "cpu_temp",
    [AlertGpuTemp] = "gpu_temp",
    [AlertCpuLoad] = "cpu_load",
    [AlertGpuLoad] = "gpu_load",
    [AlertRamLoad] = "ram_load",
    [AlertVramLoad] = "vram_load",
    [AlertDiskLoad] = "disk_load",
    [AlertBatteryLow] = "battery_low",
    [AlertLinkLost] = "link_lost",
};

void phr_settings_defaults(PhrSettings* s, AlertEngine* alerts) {
    s->transport = PhrTransportBle;
    alerts_set_defaults(alerts);
}

void phr_settings_load(Storage* storage, PhrSettings* s, AlertEngine* alerts) {
    phr_settings_defaults(s, alerts);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    do {
        if(!flipper_format_file_open_existing(ff, PHR_SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version)) break;
        if(furi_string_cmp_str(tmp, SETTINGS_HEADER) != 0 || version != SETTINGS_VERSION) {
            FURI_LOG_W(TAG, "Unknown settings header, using defaults");
            break;
        }

        uint32_t v = 0;
        flipper_format_rewind(ff); // always search from the start: order-independent
        if(flipper_format_read_uint32(ff, "transport", &v, 1) && v < PhrTransportCount)
            s->transport = (uint8_t)v;

        for(int i = 0; i < AlertRuleCount; i++) {
            // value layout: enabled, threshold, signal
            uint32_t r[3] = {0, 0, 0};
            flipper_format_rewind(ff);
            if(!flipper_format_read_uint32(ff, rule_keys[i], r, 3)) {
                continue; // key missing or malformed: keep defaults for this rule
            }
            const AlertRuleMeta* m = alerts_meta((AlertRuleId)i);
            AlertRuleConfig* c = &alerts->cfg[i];
            if(r[0] > 1 || r[1] < m->min || r[1] > m->max || r[2] >= AlertSignalCount) {
                FURI_LOG_W(TAG, "Bad values for %s, using defaults", rule_keys[i]);
                continue;
            }
            c->enabled = r[0] != 0;
            c->threshold = (uint8_t)r[1];
            c->signal = (uint8_t)r[2];
        }
    } while(false);
    furi_string_free(tmp);
    flipper_format_free(ff);
}

bool phr_settings_save(Storage* storage, const PhrSettings* s, const AlertEngine* alerts) {
    bool ok = false;
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_always(ff, PHR_SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_HEADER, SETTINGS_VERSION)) break;
        uint32_t t = s->transport;
        if(!flipper_format_write_uint32(ff, "transport", &t, 1)) break;
        bool good = true;
        for(int i = 0; i < AlertRuleCount && good; i++) {
            const AlertRuleConfig* c = &alerts->cfg[i];
            uint32_t r[3] = {c->enabled ? 1 : 0, c->threshold, c->signal};
            good = flipper_format_write_uint32(ff, rule_keys[i], r, 3);
        }
        ok = good;
    } while(false);
    flipper_format_free(ff);
    if(!ok) FURI_LOG_E(TAG, "Failed to save settings");
    return ok;
}
