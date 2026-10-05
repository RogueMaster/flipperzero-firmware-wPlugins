#include "dnd_charactersheet_api.h"
#include "dnd_profile_handoff.h"
#include "dnd_storage.h"
#include <gui/gui.h>
#include <stdlib.h>

int32_t dndcharactersheet_app(void* context) {
    UNUSED(context);
    Gui* gui = furi_record_open(RECORD_GUI);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ViewDispatcher* dispatcher = view_dispatcher_alloc();
    DndSaveData* data = calloc(1, sizeof(DndSaveData));
    DndPlugin plugin = {0};
    DndPluginLoading loading = {0};
    DndPluginUiResult result = DndPluginUiError;
    if(!gui || !storage || !dispatcher || !data) goto cleanup;
    dnd_data_set_defaults(data);
    uint32_t profile = 0;
    bool have_profile = dnd_profile_ref_active_exact(storage, &profile) &&
                        dnd_storage_load_profile(storage, profile, data, NULL);
    view_dispatcher_attach_to_gui(dispatcher, gui, ViewDispatcherTypeFullscreen);
    dnd_plugin_loading_begin(&loading, dispatcher, storage);
    if(loading.view) dnd_handoff_ready(DNDCHARACTERSHEET_FAP_PATH);
    DndPluginLoadResult loaded = dnd_plugin_open(
        &plugin,
        storage,
        DND_CHARACTER_SHEET_STANDALONE_PATH,
        DND_CHARACTER_SHEET_API_ID,
        DND_CHARACTER_SHEET_API_VERSION,
        sizeof(DndCharacterSheetApi));
    const DndCharacterSheetApi* api = plugin.api;
    if(loaded == DndPluginLoadOk && api->run)
        result =
            api->run(dispatcher, have_profile ? &data->character : NULL, loading.view != NULL);
    else
        dnd_plugin_show_error(dispatcher, "Character Sheet", loaded, loading.view != NULL);
    if(result == DndPluginUiReturn)
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_CHARACTER);
cleanup:
    dnd_plugin_loading_end(&loading, dispatcher);
    dnd_plugin_close(&plugin);
    if(data) {
        dnd_data_clear(data);
        free(data);
    }
    if(dispatcher) view_dispatcher_free(dispatcher);
    if(storage) furi_record_close(RECORD_STORAGE);
    if(gui) furi_record_close(RECORD_GUI);
    return result == DndPluginUiError ? -1 : 0;
}
