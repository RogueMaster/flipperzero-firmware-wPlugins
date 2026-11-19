#include "subghz_frequency_analyzer_plugin.h"

#include "../subghz_i.h"
#include "subghz_feature_plugin.h"

#define SUBGHZ_FREQUENCY_ANALYZER_PLUGIN_PATH \
    EXT_PATH("apps_data/subghz/plugins/subghz_frequency_analyzer.fal")

bool subghz_frequency_analyzer_plugin_load(SubGhz* subghz) {
    furi_assert(!subghz->freq_analyzer_plugin_manager);

    subghz->freq_analyzer_plugin_manager = subghz_feature_plugin_load(
        subghz, SUBGHZ_FREQUENCY_ANALYZER_PLUGIN_APP_ID, SUBGHZ_FREQUENCY_ANALYZER_PLUGIN_PATH);
    if(!subghz->freq_analyzer_plugin_manager) {
        return false;
    }

    subghz->freq_analyzer_plugin = plugin_manager_get_ep(subghz->freq_analyzer_plugin_manager, 0);
    subghz->freq_analyzer_plugin->on_enter(subghz);
    return true;
}

void subghz_frequency_analyzer_plugin_unload(SubGhz* subghz) {
    if(subghz->freq_analyzer_plugin) {
        // Joins the analyzer's worker thread, which runs plugin code, before we unmap it.
        subghz->freq_analyzer_plugin->on_exit(subghz);
        subghz->freq_analyzer_plugin = NULL;
    }
    subghz_feature_plugin_unload(subghz, subghz->freq_analyzer_plugin_manager);
    subghz->freq_analyzer_plugin_manager = NULL;
}
