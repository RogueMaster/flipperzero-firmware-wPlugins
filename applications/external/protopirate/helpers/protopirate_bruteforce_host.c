#include "../protopirate_app_i.h"
#include "protopirate_bruteforce_host.h"
#include "../protopirate_history.h"
#include "../protocols/protocols_common.h"
#include "../scenes/plugins/protopirate_bruteforce_plugin.h"

#include <notification/notification_messages.h>

#define TAG "PPBruteForceHost"

bool protopirate_bruteforce_plugin_ensure_loaded(ProtoPirateApp* app) {
    if(shared_plugin_load(
           (void**)&app->bruteforce_plugin_flipper_application,
           (const void**)&app->bruteforce_plugin,
           ProtoPirateSharedPluginsPSABruteforce,
           NULL)) {
        app->bruteforce_plugin->set_host_api(&protopirate_shared_plugin_host_api);
        return true;
    } else {
        return false;
    }
}
void protopirate_bruteforce_plugin_unload_if_idle(ProtoPirateApp* context) {
    ProtoPirateApp* app = context;
    if(!app) return;
    if(app->bruteforce_plugin && app->bruteforce_plugin->is_running &&
       app->bruteforce_plugin->is_running(app)) {
        return;
    }

    shared_plugin_unload(
        (void**)&app->bruteforce_plugin_flipper_application,
        (const void**)&app->bruteforce_plugin);
}

void protopirate_bruteforce_context_release(ProtoPirateApp* app) {
    if(!app) return;
    if(app->bruteforce_plugin && app->bruteforce_plugin->release) {
        app->bruteforce_plugin->release(app);
    }
    shared_plugin_unload(
        (void**)&app->bruteforce_plugin_flipper_application,
        (const void**)&app->bruteforce_plugin);
}
