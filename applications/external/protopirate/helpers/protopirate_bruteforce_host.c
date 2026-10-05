#include "protopirate_bruteforce_host.h"
#include "../protopirate_app_i.h"

#include <notification/notification_messages.h>

#define TAG "PPBruteForceHost"

bool protopirate_bruteforce_plugin_ensure_loaded(ProtoPirateApp* app) {
    if(shared_plugin_load(
           &app->bruteforce_plugin_flipper_application,
           &app->running_bruteforce_plugin,
           ProtoPirateSharedPluginsPSABruteforce,
           NULL)) {
        app->running_bruteforce_plugin.bruteforce_plugin->set_host_api(
            &protopirate_shared_plugin_host_api);
        return true;
    } else {
        return false;
    }
}
void protopirate_bruteforce_plugin_unload_if_idle(ProtoPirateApp* context) {
    ProtoPirateApp* app = context;
    if(!app) return;
    if(app->running_bruteforce_plugin.bruteforce_plugin &&
       app->running_bruteforce_plugin.bruteforce_plugin->is_running &&
       app->running_bruteforce_plugin.bruteforce_plugin->is_running(app)) {
        return;
    }

    shared_plugin_unload(
        &app->bruteforce_plugin_flipper_application, &app->running_bruteforce_plugin);
}

void protopirate_bruteforce_context_release(ProtoPirateApp* app) {
    if(!app) return;
    if(app->running_bruteforce_plugin.bruteforce_plugin &&
       app->running_bruteforce_plugin.bruteforce_plugin->release) {
        app->running_bruteforce_plugin.bruteforce_plugin->release(app);
    }
    shared_plugin_unload(
        &app->bruteforce_plugin_flipper_application, &app->running_bruteforce_plugin);
}
