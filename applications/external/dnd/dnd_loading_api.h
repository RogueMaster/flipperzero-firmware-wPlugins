#pragma once
#include <gui/view.h>
#include <loader/loader.h>
#include <stdint.h>
#define DND_LOADING_API_ID         "dnd_loading"
#define DND_LOADING_API_VERSION    2U
#define DND_LOADING_PLUGIN_PATH    "/ext/apps_data/dndolphins/plugins/dnd_loading.fal"
#define DND_LOADING_HANDOFF_RECORD "dnd_loading_handoff_v2"
typedef struct {
    uint32_t size;
    void* (*alloc)(void);
    View* (*get_view)(void* context);
    void (*free)(void* context);
    void* (*handoff_begin)(Loader* loader);
    bool (*handoff_active)(void* context);
    void (*handoff_end)(void* context);
} DndLoadingApi;
