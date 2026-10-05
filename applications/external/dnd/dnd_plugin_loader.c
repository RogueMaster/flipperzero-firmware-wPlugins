#include "dnd_plugin_loader.h"
#include "dnd_loading_api.h"
#include <loader/firmware_api/firmware_api.h>
#include <gui/view.h>
#include <input/input.h>
#include <loader/loader.h>
#include <string.h>

static void dnd_plugin_loader_barrier(void) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    if(loader) {
        (void)loader_is_locked(loader);
        furi_record_close(RECORD_LOADER);
    }
}

void dnd_plugin_close(DndPlugin* plugin) {
    if(!plugin) return;
    plugin->api = NULL;
    if(plugin->application) {
        dnd_plugin_loader_barrier();
        flipper_application_free(plugin->application);
    }
    plugin->application = NULL;
}

DndPluginLoadResult dnd_plugin_open(
    DndPlugin* plugin,
    Storage* storage,
    const char* path,
    const char* api_id,
    uint32_t api_version,
    uint32_t api_size) {
    if(!plugin || !storage || !path || !api_id || api_size < sizeof(uint32_t) ||
       plugin->application)
        return DndPluginLoadInvalid;
    plugin->api = NULL;
    dnd_plugin_loader_barrier();
    if(!storage_file_exists(storage, path)) return DndPluginLoadMissing;
    plugin->application = flipper_application_alloc(storage, firmware_api_interface);
    if(!plugin->application) return DndPluginLoadMemory;
    DndPluginLoadResult result = DndPluginLoadInvalid;
    FlipperApplicationPreloadStatus preload =
        flipper_application_preload(plugin->application, path);
    if(preload != FlipperApplicationPreloadStatusSuccess) {
        if(preload == FlipperApplicationPreloadStatusNotEnoughMemory)
            result = DndPluginLoadMemory;
        else if(
            preload == FlipperApplicationPreloadStatusApiTooOld ||
            preload == FlipperApplicationPreloadStatusApiTooNew)
            result = DndPluginLoadIncompatible;
        goto fail;
    }
    if(!flipper_application_is_plugin(plugin->application)) goto fail;
    FlipperApplicationLoadStatus mapped = flipper_application_map_to_memory(plugin->application);
    if(mapped != FlipperApplicationLoadStatusSuccess) {
        if(mapped == FlipperApplicationLoadStatusMissingImports)
            result = DndPluginLoadIncompatible;
        goto fail;
    }
    const FlipperAppPluginDescriptor* descriptor =
        flipper_application_plugin_get_descriptor(plugin->application);
    if(!descriptor || !descriptor->appid || !descriptor->entry_point ||
       strcmp(descriptor->appid, api_id) || descriptor->ep_api_version != api_version) {
        result = DndPluginLoadIncompatible;
        goto fail;
    }
    uint32_t provided_size;
    memcpy(&provided_size, descriptor->entry_point, sizeof(provided_size));
    if(provided_size < api_size) {
        result = DndPluginLoadIncompatible;
        goto fail;
    }
    plugin->api = descriptor->entry_point;
    return DndPluginLoadOk;
fail:
    dnd_plugin_close(plugin);
    return result;
}

const char* dnd_plugin_load_message(DndPluginLoadResult result) {
    switch(result) {
    case DndPluginLoadMissing:
        return "FAL missing - install suite";
    case DndPluginLoadMemory:
        return "Not enough memory for FAL";
    case DndPluginLoadIncompatible:
        return "FAL / firmware API mismatch";
    default:
        return "FAL load failed - retry";
    }
}

typedef struct {
    ViewDispatcher* dispatcher;
    const char* title;
    const char* message;
} DndPluginError;
#define DND_PLUGIN_ERROR_VIEW 0xDFFFU
static void dnd_plugin_error_draw(Canvas* canvas, void* model) {
    DndPluginError* error = *(DndPluginError**)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 16, error->title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 34, error->message);
    canvas_draw_str(canvas, 2, 58, "Back: return");
}
static bool dnd_plugin_error_input(InputEvent* event, void* context) {
    DndPluginError* error = context;
    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong))
        view_dispatcher_stop(error->dispatcher);
    return true;
}
void dnd_plugin_show_error(
    ViewDispatcher* dispatcher,
    const char* title,
    DndPluginLoadResult result,
    bool loading_on_return) {
    if(!dispatcher) return;
    View* view = view_alloc();
    if(!view) return;
    DndPluginError error = {dispatcher, title, dnd_plugin_load_message(result)};
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(DndPluginError*));
    DndPluginError** model = view_get_model(view);
    if(!model) {
        view_free(view);
        return;
    }
    *model = &error;
    view_set_context(view, &error);
    view_set_draw_callback(view, dnd_plugin_error_draw);
    view_set_input_callback(view, dnd_plugin_error_input);
    dnd_plugin_clear_dispatcher(dispatcher);
    view_dispatcher_add_view(dispatcher, DND_PLUGIN_ERROR_VIEW, view);
    view_dispatcher_switch_to_view(dispatcher, DND_PLUGIN_ERROR_VIEW);
    view_dispatcher_run(dispatcher);
    if(loading_on_return) dnd_plugin_ui_return_loading(dispatcher);
    view_dispatcher_remove_view(dispatcher, DND_PLUGIN_ERROR_VIEW);
    view_free(view);
}

static void dnd_plugin_loading_fallback_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 24, 34, "Loading...");
}
static bool dnd_plugin_loading_fallback_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return true;
}
void dnd_plugin_loading_begin(
    DndPluginLoading* loading,
    ViewDispatcher* dispatcher,
    Storage* storage) {
    if(!loading || !dispatcher || loading->plugin.application) return;
    DndPluginLoadResult result = dnd_plugin_open(
        &loading->plugin,
        storage,
        DND_LOADING_PLUGIN_PATH,
        DND_LOADING_API_ID,
        DND_LOADING_API_VERSION,
        sizeof(DndLoadingApi));
    if(result != DndPluginLoadOk) goto fallback;
    const DndLoadingApi* api = loading->plugin.api;
    if(!api->alloc || !api->get_view || !api->free) goto fail;
    loading->context = api->alloc();
    if(!loading->context) goto fail;
    loading->view = api->get_view(loading->context);
    if(!loading->view) goto fail;
    view_dispatcher_add_view(dispatcher, DND_PLUGIN_LOADING_VIEW, loading->view);
    view_dispatcher_switch_to_view(dispatcher, DND_PLUGIN_LOADING_VIEW);
    return;
fail:
    if(loading->context) api->free(loading->context);
    loading->context = NULL;
    loading->view = NULL;
    dnd_plugin_close(&loading->plugin);
fallback:
    loading->view = view_alloc();
    if(!loading->view) return;
    loading->fallback = true;
    view_set_draw_callback(loading->view, dnd_plugin_loading_fallback_draw);
    view_set_input_callback(loading->view, dnd_plugin_loading_fallback_input);
    view_dispatcher_add_view(dispatcher, DND_PLUGIN_LOADING_VIEW, loading->view);
    view_dispatcher_switch_to_view(dispatcher, DND_PLUGIN_LOADING_VIEW);
}
void dnd_plugin_loading_end(DndPluginLoading* loading, ViewDispatcher* dispatcher) {
    if(!loading) return;
    if(loading->view) view_dispatcher_remove_view(dispatcher, DND_PLUGIN_LOADING_VIEW);
    if(loading->fallback && loading->view) view_free(loading->view);
    if(loading->context) {
        const DndLoadingApi* api = loading->plugin.api;
        api->free(loading->context);
    }
    loading->context = NULL;
    loading->view = NULL;
    loading->fallback = false;
    dnd_plugin_close(&loading->plugin);
}
