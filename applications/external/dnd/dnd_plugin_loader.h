#pragma once
#include <flipper_application/flipper_application.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <storage/storage.h>

typedef struct {
    FlipperApplication* application;
    const void* api;
} DndPlugin;

typedef enum {
    DndPluginLoadOk,
    DndPluginLoadMissing,
    DndPluginLoadMemory,
    DndPluginLoadInvalid,
    DndPluginLoadIncompatible,
} DndPluginLoadResult;

typedef enum {
    DndPluginUiError = -1,
    DndPluginUiReturn = 0,
    DndPluginUiExit = 1,
    DndPluginUiAdventure = 2,
} DndPluginUiResult;

DndPluginLoadResult dnd_plugin_open(
    DndPlugin* plugin,
    Storage* storage,
    const char* path,
    const char* api_id,
    uint32_t api_version,
    uint32_t api_size);
void dnd_plugin_close(DndPlugin* plugin);
const char* dnd_plugin_load_message(DndPluginLoadResult result);
void dnd_plugin_show_error(
    ViewDispatcher* dispatcher,
    const char* title,
    DndPluginLoadResult result,
    bool loading_on_return);

static inline void dnd_plugin_clear_dispatcher(ViewDispatcher* dispatcher) {
    view_dispatcher_set_custom_event_callback(dispatcher, NULL);
    view_dispatcher_set_navigation_event_callback(dispatcher, NULL);
    view_dispatcher_set_tick_event_callback(dispatcher, NULL, 0);
    view_dispatcher_set_event_callback_context(dispatcher, NULL);
}

#define DND_PLUGIN_LOADING_VIEW 0xDFFEU
typedef struct {
    DndPlugin plugin;
    void* context;
    View* view;
    bool fallback;
} DndPluginLoading;
void dnd_plugin_loading_begin(
    DndPluginLoading* loading,
    ViewDispatcher* dispatcher,
    Storage* storage);
void dnd_plugin_loading_end(DndPluginLoading* loading, ViewDispatcher* dispatcher);
static inline void dnd_plugin_ui_return_loading(ViewDispatcher* dispatcher) {
    view_dispatcher_switch_to_view(dispatcher, DND_PLUGIN_LOADING_VIEW);
}
