#include "dnd_journal_api.h"
#include "dnd_profile_handoff.h"
#include <gui/gui.h>

int32_t dndjournal_app(void* context) {
    UNUSED(context);
    Gui* gui = furi_record_open(RECORD_GUI);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ViewDispatcher* dispatcher = view_dispatcher_alloc();
    DndPlugin plugin = {0};
    DndPluginLoading loading = {0};
    DndPluginUiResult result = DndPluginUiError;
    if(!gui || !storage || !dispatcher) goto cleanup;
    uint32_t profile = 0;
    bool have_profile = dnd_profile_ref_active_exact(storage, &profile);
    view_dispatcher_attach_to_gui(dispatcher, gui, ViewDispatcherTypeFullscreen);
    dnd_plugin_loading_begin(&loading, dispatcher, storage);
    if(loading.view) dnd_handoff_ready(DNDJOURNAL_FAP_PATH);
    DndPluginLoadResult loaded = dnd_plugin_open(
        &plugin,
        storage,
        DND_JOURNAL_STANDALONE_PATH,
        DND_JOURNAL_API_ID,
        DND_JOURNAL_API_VERSION,
        sizeof(DndJournalApi));
    const DndJournalApi* api = plugin.api;
    if(loaded == DndPluginLoadOk && api->run)
        result = api->run(dispatcher, storage, profile, have_profile, loading.view != NULL);
    else
        dnd_plugin_show_error(dispatcher, "Journal", loaded, loading.view != NULL);
    if(result == DndPluginUiAdventure)
        (void)dnd_handoff_launch(DNDADVENTURE_FAP_PATH, DND_PROFILE_HANDOFF_ADVENTURE_CONTINUE);
    else if(result == DndPluginUiReturn)
        (void)dnd_handoff_launch_if_present(DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_JOURNAL);
cleanup:
    dnd_plugin_loading_end(&loading, dispatcher);
    dnd_plugin_close(&plugin);
    if(dispatcher) view_dispatcher_free(dispatcher);
    if(storage) furi_record_close(RECORD_STORAGE);
    if(gui) furi_record_close(RECORD_GUI);
    return result == DndPluginUiError ? -1 : 0;
}
