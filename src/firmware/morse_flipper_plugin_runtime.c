/*
 * Purpose: Own the shared embedded-plugin lifecycle lock.
 * Owns: The app-lifetime mutex used to serialize plugin calls and unload.
 */

#include "morse_flipper_app_i.h"

bool morse_flipper_plugin_runtime_init(MorseFlipperApp* app) {
    if(app == NULL) return false;

    app->plugin_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return app->plugin_mutex != NULL;
}

void morse_flipper_plugin_runtime_deinit(MorseFlipperApp* app) {
    if(app == NULL) return;

    /* The live view has already been detached by the caller. */
    morse_flipper_icr_host_unload(app);
    morse_flipper_content_host_unload(app);
    if(app->plugin_mutex != NULL) {
        furi_mutex_free(app->plugin_mutex);
        app->plugin_mutex = NULL;
    }
}
