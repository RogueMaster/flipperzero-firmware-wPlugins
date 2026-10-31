#include "flippass.h"
#include "flippass_db.h"
#include "flippass_crypto.h"
#include "scenes/flippass_scene_editor.h"
#include "plugins/flippass_open_host_plugin.h"

/* The unlock pipeline lives in the open_host plugin so its code is resident only
 * while a database is being opened.
 *
 * Anti-fragmentation handover: open_host is loaded first into the freshly trimmed
 * heap and stays mapped, unmoved, through the whole open, while the open's own
 * allocations fragment everything around it. Its .text and .rodata carry a
 * "successor room" (see plugins/flippass_open_host_plugin.c) sized so the two blocks
 * it frees can take the editor's entire load sequence. The editor is therefore loaded
 * right after open_host unloads, with no allocation in between, instead of being kept
 * resident through the open. */
bool flippass_open_execute(App* app, FuriString* error) {
    furi_assert(app);
    furi_assert(error);

    /* Start from a trimmed heap so open_host takes the lowest large free block. */
    flippass_output_cleanup(app);
    flippass_module_unload_all_runtime(app);
    flippass_editor_end_database_session(app);

    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOpenHost,
        NULL,
        FLIPPASS_OPEN_HOST_PLUGIN_APP_ID,
        FLIPPASS_OPEN_HOST_PLUGIN_API_VERSION,
        error);
    const FlipPassOpenHostPluginV2* plugin = (descriptor != NULL) ? descriptor->entry_point : NULL;
    if(plugin == NULL || plugin->api_version != FLIPPASS_OPEN_HOST_PLUGIN_API_VERSION ||
       plugin->execute == NULL || plugin->get_crypto_api == NULL) {
        flippass_module_unload(app, FlipPassModuleSlotOpenHost);
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "Not enough RAM is available to load the unlock module.");
        }
        return false;
    }

    flippass_module_pin(app, FlipPassModuleSlotOpenHost);
    const bool bound = flippass_crypto_begin_scope(plugin->get_crypto_api());
    const bool opened = bound && plugin->execute(app, error);
    if(bound)
        flippass_crypto_end_scope();
    else
        furi_string_set_str(error, "Unlock crypto provider is unavailable.");
    flippass_module_unpin(app, FlipPassModuleSlotOpenHost);

    /* Handover: nothing may allocate between these two calls, or it could split the
     * room open_host leaves behind. A failed editor load never fails the open: the
     * editor scenes retry on demand and report "Editor Unavailable" if they must. */
    flippass_module_unload(app, FlipPassModuleSlotOpenHost);
    if(opened && !app->save_uncompressed_requested && app->launch_request == NULL) {
        flippass_editor_begin_database_session(app);
        flippass_db_refresh_model_budget(app);
    }
    return opened;
}
