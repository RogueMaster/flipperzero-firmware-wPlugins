#include "dnd_profile_handoff.h"
#include "dnd_plugin_loader.h"
#include "dnd_loading_api.h"

#include <furi.h>
#include <loader/loader.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DndPlugin plugin;
    Storage* storage;
    Loader* loader;
    void* context;
    char target[128];
} DndLoadingTransfer;

static void dnd_handoff_transfer_free(DndLoadingTransfer* transfer) {
    const DndLoadingApi* api = transfer->plugin.api;
    if(transfer->context) api->handoff_end(transfer->context);
    dnd_plugin_close(&transfer->plugin);
    if(transfer->storage) furi_record_close(RECORD_STORAGE);
    if(transfer->loader) furi_record_close(RECORD_LOADER);
    free(transfer);
}
static void dnd_handoff_transfer_destroy(DndLoadingTransfer* transfer) {
    furi_record_close(DND_LOADING_HANDOFF_RECORD);
    /* Only the DND app thread opens this record; callbacks never open it. */
    furi_check(furi_record_destroy(DND_LOADING_HANDOFF_RECORD));
    dnd_handoff_transfer_free(transfer);
}
static void dnd_handoff_transfer_prepare(Loader* loader, const char* path) {
    size_t length = strlen(path);
    if(length >= sizeof(((DndLoadingTransfer*)0)->target)) return;
    if(furi_record_exists(DND_LOADING_HANDOFF_RECORD)) {
        DndLoadingTransfer* previous = furi_record_open(DND_LOADING_HANDOFF_RECORD);
        dnd_handoff_transfer_destroy(previous);
    }
    DndLoadingTransfer* transfer = calloc(1, sizeof(DndLoadingTransfer));
    if(!transfer) return;
    transfer->storage = furi_record_open(RECORD_STORAGE);
    transfer->loader = furi_record_open(RECORD_LOADER);
    if(!transfer->storage || !transfer->loader) goto fail;
    if(dnd_plugin_open(
           &transfer->plugin,
           transfer->storage,
           DND_LOADING_PLUGIN_PATH,
           DND_LOADING_API_ID,
           DND_LOADING_API_VERSION,
           sizeof(DndLoadingApi)) != DndPluginLoadOk)
        goto fail;
    const DndLoadingApi* api = transfer->plugin.api;
    if(!api->handoff_begin || !api->handoff_active || !api->handoff_end) goto fail;
    transfer->context = api->handoff_begin(loader);
    if(!transfer->context) goto fail;
    memcpy(transfer->target, path, length + 1U);
    /* All surviving callbacks and artwork belong to the retained FAL, never
     * to the outgoing FAP. The incoming app owns normal cleanup/unmap. */
    furi_record_create(DND_LOADING_HANDOFF_RECORD, transfer);
    return;
fail:
    dnd_handoff_transfer_free(transfer);
}
bool dnd_handoff_launch(const char* path, const char* args) {
    if(!path || !path[0]) return false;
    Loader* loader = furi_record_open(RECORD_LOADER);
    if(!loader) return false;
    dnd_handoff_transfer_prepare(loader, path);
    loader_enqueue_launch(loader, path, args, LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);
    return true;
}
bool dnd_handoff_launch_if_present(const char* path, const char* args) {
    if(!path || !path[0]) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    bool present = storage_file_exists(storage, path);
    furi_record_close(RECORD_STORAGE);
    return present && dnd_handoff_launch(path, args);
}
void dnd_handoff_ready(const char* current_fap_path) {
    /* This synchronous public request also waits for Loader's startup work to
     * finish before any FAL map/free touches the SDK's loaded-module list. */
    Loader* loader = furi_record_open(RECORD_LOADER);
    if(!loader) return;
    (void)loader_is_locked(loader);
    if(furi_record_exists(DND_LOADING_HANDOFF_RECORD)) {
        DndLoadingTransfer* transfer = furi_record_open(DND_LOADING_HANDOFF_RECORD);
        const DndLoadingApi* api = transfer->plugin.api;
        bool target = current_fap_path && !strcmp(current_fap_path, transfer->target);
        if(target || !api->handoff_active(transfer->context))
            dnd_handoff_transfer_destroy(transfer);
        else
            furi_record_close(DND_LOADING_HANDOFF_RECORD);
    }
    furi_record_close(RECORD_LOADER);
}
