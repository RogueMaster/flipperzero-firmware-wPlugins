#include "gta_handoff.h"
#include <furi.h>
#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

struct GtaLoadingHandle {
    FlipperApplication* module;
    Storage* storage;
    const GtaLoadingApi* api;
    void* context;
};

void gta_loading_hide(GtaLoadingHandle* handle) {
    if(!handle) return;
    if(handle->context) handle->api->end(handle->context);
    if(handle->module) flipper_application_free(handle->module);
    if(handle->storage) furi_record_close(RECORD_STORAGE);
    free(handle);
}

GtaLoadingHandle* gta_loading_show(void) {
    GtaLoadingHandle* handle = calloc(1, sizeof(*handle));
    if(!handle) return NULL;
    handle->storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(handle->storage, GTA_LOADING_PATH)) goto fail;
    handle->module = flipper_application_alloc(handle->storage, firmware_api_interface);
    if(!handle->module) goto fail;
    if(flipper_application_preload(handle->module, GTA_LOADING_PATH) !=
       FlipperApplicationPreloadStatusSuccess)
        goto fail;
    if(!flipper_application_is_plugin(handle->module)) goto fail;
    if(flipper_application_map_to_memory(handle->module) != FlipperApplicationLoadStatusSuccess)
        goto fail;
    const FlipperAppPluginDescriptor* d =
        flipper_application_plugin_get_descriptor(handle->module);
    if(!d || !d->appid || strcmp(d->appid, GTA_LOADING_ID) ||
       d->ep_api_version != GTA_LOADING_API_VERSION || !d->entry_point)
        goto fail;
    handle->api = d->entry_point;
    if(handle->api->size < sizeof(GtaLoadingApi) || !handle->api->begin || !handle->api->end)
        goto fail;
    handle->context = handle->api->begin();
    if(!handle->context) goto fail;
    return handle;
fail:
    gta_loading_hide(handle);
    return NULL;
}
