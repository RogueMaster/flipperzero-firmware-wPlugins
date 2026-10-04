#pragma once
#include <furi.h>
#include <toolbox/api_lock.h>
#include <flipper_application/flipper_application.h>

#include <gui/gui.h>
#include <gui/view_holder.h>
#include <gui/modules/loading.h>

#include <m-array.h>

#include "loader.h"
#include "loader_menu.h"
#include "loader_applications.h"
#include "loader_queue.h"

typedef struct {
    FuriString* launch_path;
    char* args;
    FuriThread* thread;
    bool insomniac;
    bool rpc;
    FlipperApplication* fap;

    bool unloaded_asset_packs;
} LoaderAppData;

struct Loader {
    FuriPubSub* pubsub;
    FuriMessageQueue* queue;
    LoaderMenu* loader_menu;
    LoaderApplications* loader_applications;
    LoaderAppData app;

    LoaderLaunchQueue launch_queue;

    FuriMutex* menu_style_mutex;
    char menu_style_name[32];
    uint32_t menu_style_setting;

    Gui* gui;
    ViewHolder* view_holder;
    Loading* loading;
    uint8_t loading_depth;
    FuriTimer* loading_timer;
    uint32_t loading_hold_start;
    size_t loading_view_ports_baseline;
    bool loading_held;
};

/** Copy the filename selected for the next primary menu into a 32-byte buffer. */
void loader_get_menu_style_name(Loader* loader, char name[32]);

typedef enum {
    LoaderMessageTypeStartByName,
    LoaderMessageTypeAppClosed,
    LoaderMessageTypeShowMenu,
    LoaderMessageTypeMenuClosed,
    LoaderMessageTypeApplicationsClosed,
    LoaderMessageTypeLock,
    LoaderMessageTypeUnlock,
    LoaderMessageTypeIsLocked,
    LoaderMessageTypeStartByNameDetachedWithGuiError,
    LoaderMessageTypeSignal,
    LoaderMessageTypeGetApplicationName,
    LoaderMessageTypeGetApplicationLaunchPath,
    LoaderMessageTypeEnqueueLaunch,
    LoaderMessageTypeClearLaunchQueue,

    LoaderMessageTypeShowSettings,
    LoaderMessageTypeSetMenuStyle,
    LoaderMessageTypeLoadingCheck,
} LoaderMessageType;

typedef struct {
    const char* name;
    const char* args;
    FuriString* error_message;
} LoaderMessageStartByName;

typedef struct {
    uint32_t signal;
    void* arg;
} LoaderMessageSignal;

typedef enum {
    LoaderStatusErrorUnknown,
    LoaderStatusErrorInvalidFile,
    LoaderStatusErrorInvalidManifest,
    LoaderStatusErrorMissingImports,
    LoaderStatusErrorHWMismatch,
    LoaderStatusErrorOutdatedApp,
    LoaderStatusErrorOutOfMemory,
    LoaderStatusErrorOutdatedFirmware,
    LoaderStatusErrorMissingRuntime,
} LoaderStatusError;

typedef struct {
    LoaderStatus value;
    LoaderStatusError error;
} LoaderMessageLoaderStatusResult;

typedef struct {
    bool value;
} LoaderMessageBoolResult;

typedef struct {
    FuriApiLock api_lock;
    LoaderMessageType type;

    union {
        LoaderMessageStartByName start;
        LoaderDeferredLaunchRecord defer_start;
        LoaderMessageSignal signal;
        FuriString* application_name;
        char* menu_style_name;
    };

    union {
        LoaderMessageLoaderStatusResult* status_value;
        LoaderMessageBoolResult* bool_value;
    };
} LoaderMessage;
