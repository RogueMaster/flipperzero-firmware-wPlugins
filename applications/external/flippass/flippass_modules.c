#include "flippass.h"

#include <loader/firmware_api/firmware_api.h>

#include <string.h>

#define FLIPPASS_OUTPUT_USB_PLUGIN_PATH   APP_ASSETS_PATH("plugins/flippass_output_usb.fal")
#define FLIPPASS_OUTPUT_BLE_PLUGIN_PATH   APP_ASSETS_PATH("plugins/flippass_output_ble.fal")
#define FLIPPASS_RPC_COMMANDS_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_rpc_commands.fal")
#define FLIPPASS_OPEN_ACQUIRE_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_open_acquire.fal")
#define FLIPPASS_OPEN_STREAM_PLUGIN_PATH  APP_ASSETS_PATH("plugins/flippass_open_stream.fal")
#define FLIPPASS_OPEN_INFLATE_NONPAGED_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_open_inflate_nonpaged.fal")
#define FLIPPASS_OPEN_INFLATE_PAGED_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_open_inflate_paged.fal")
#define FLIPPASS_OPEN_MODEL_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_open_model.fal")
#define FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_keyboard_layout.fal")

#if FLIPPASS_ENABLE_LOGS
static const char* flippass_module_slot_name(FlipPassModuleSlot slot) {
    switch(slot) {
    case FlipPassModuleSlotOutputUsb:
        return "output_usb";
    case FlipPassModuleSlotOutputBle:
        return "output_ble";
    case FlipPassModuleSlotRpcCommands:
        return "rpc_commands";
    case FlipPassModuleSlotOpenAcquire:
        return "open_acquire";
    case FlipPassModuleSlotOpenStream:
        return "open_stream";
    case FlipPassModuleSlotOpenInflateNonPaged:
        return "open_inflate_nonpaged";
    case FlipPassModuleSlotOpenInflatePaged:
        return "open_inflate_paged";
    case FlipPassModuleSlotOpenModel:
        return "open_model";
    case FlipPassModuleSlotKeyboardLayout:
        return "keyboard_layout";
    case FlipPassModuleSlotCount:
    default:
        return "unknown";
    }
}
#endif

static const char* flippass_module_default_path(FlipPassModuleSlot slot) {
    switch(slot) {
    case FlipPassModuleSlotOutputUsb:
        return FLIPPASS_OUTPUT_USB_PLUGIN_PATH;
    case FlipPassModuleSlotOutputBle:
        return FLIPPASS_OUTPUT_BLE_PLUGIN_PATH;
    case FlipPassModuleSlotRpcCommands:
        return FLIPPASS_RPC_COMMANDS_PLUGIN_PATH;
    case FlipPassModuleSlotOpenAcquire:
        return FLIPPASS_OPEN_ACQUIRE_PLUGIN_PATH;
    case FlipPassModuleSlotOpenStream:
        return FLIPPASS_OPEN_STREAM_PLUGIN_PATH;
    case FlipPassModuleSlotOpenInflateNonPaged:
        return FLIPPASS_OPEN_INFLATE_NONPAGED_PLUGIN_PATH;
    case FlipPassModuleSlotOpenInflatePaged:
        return FLIPPASS_OPEN_INFLATE_PAGED_PLUGIN_PATH;
    case FlipPassModuleSlotOpenModel:
        return FLIPPASS_OPEN_MODEL_PLUGIN_PATH;
    case FlipPassModuleSlotKeyboardLayout:
        return FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_PATH;
    case FlipPassModuleSlotCount:
    default:
        return NULL;
    }
}

void flippass_module_loader_init(App* app) {
    furi_assert(app);

    memset(&app->module_loader, 0, sizeof(app->module_loader));
    app->module_loader.storage = furi_record_open(RECORD_STORAGE);
}

void flippass_module_unload(App* app, FlipPassModuleSlot slot) {
    furi_assert(app);
    furi_assert(slot < FlipPassModuleSlotCount);

    FlipPassModuleInstance* instance = &app->module_loader.slot[slot];
    if(instance->application != NULL) {
#if FLIPPASS_ENABLE_LOGS
        const size_t free_heap = memmgr_get_free_heap();
        const size_t max_free = memmgr_heap_get_max_free_block();
        FLIPPASS_LOG_EVENT(
            app,
            "MODULE_UNLOAD slot=%s appid=%s free=%lu max=%lu",
            flippass_module_slot_name(slot),
            (instance->descriptor != NULL && instance->descriptor->appid != NULL) ?
                instance->descriptor->appid :
                "unknown",
            (unsigned long)free_heap,
            (unsigned long)max_free);
#endif
        flipper_application_free(instance->application);
    }

    instance->application = NULL;
    instance->descriptor = NULL;
}

void flippass_module_loader_deinit(App* app) {
    furi_assert(app);

    for(size_t index = 0U; index < FlipPassModuleSlotCount; index++) {
        flippass_module_unload(app, (FlipPassModuleSlot)index);
    }

    if(app->module_loader.storage != NULL) {
        furi_record_close(RECORD_STORAGE);
        app->module_loader.storage = NULL;
    }
}

const FlipperAppPluginDescriptor* flippass_module_ensure(
    App* app,
    FlipPassModuleSlot slot,
    const char* path,
    const char* expected_appid,
    uint32_t expected_api_version,
    FuriString* error) {
    furi_assert(app);
    furi_assert(slot < FlipPassModuleSlotCount);

    FlipPassModuleInstance* instance = &app->module_loader.slot[slot];
    if(instance->descriptor != NULL) {
        return instance->descriptor;
    }

    if(app->module_loader.storage == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass plugin storage is unavailable.");
        }
        return NULL;
    }

    const char* module_path = (path != NULL) ? path : flippass_module_default_path(slot);
    if(module_path == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass plugin path is not defined.");
        }
        return NULL;
    }

    FlipperApplication* application =
        flipper_application_alloc(app->module_loader.storage, firmware_api_interface);
    if(application == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not allocate the requested plugin.");
        }
        return NULL;
    }

    const FlipperApplicationPreloadStatus preload_status =
        flipper_application_preload(application, module_path);
    if(preload_status != FlipperApplicationPreloadStatusSuccess) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "FlipPass could not preload %s (%s).",
                module_path,
                flipper_application_preload_status_to_string(preload_status));
        }
        flipper_application_free(application);
        return NULL;
    }

    if(!flipper_application_is_plugin(application)) {
        if(error != NULL) {
            furi_string_printf(error, "%s is not a FlipPass plugin.", module_path);
        }
        flipper_application_free(application);
        return NULL;
    }

    const FlipperApplicationLoadStatus load_status =
        flipper_application_map_to_memory(application);
    if(load_status != FlipperApplicationLoadStatusSuccess) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "FlipPass could not map %s (%s).",
                module_path,
                flipper_application_load_status_to_string(load_status));
        }
        flipper_application_free(application);
        return NULL;
    }

    const FlipperAppPluginDescriptor* descriptor =
        flipper_application_plugin_get_descriptor(application);
    if(descriptor == NULL) {
        if(error != NULL) {
            furi_string_printf(error, "%s did not expose a plugin descriptor.", module_path);
        }
        flipper_application_free(application);
        return NULL;
    }

    if(expected_appid != NULL && strcmp(descriptor->appid, expected_appid) != 0) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "FlipPass loaded %s but received plugin appid %s.",
                module_path,
                descriptor->appid);
        }
        flipper_application_free(application);
        return NULL;
    }

    if(expected_api_version != 0U && descriptor->ep_api_version != expected_api_version) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "FlipPass plugin %s uses API %lu instead of %lu.",
                descriptor->appid,
                (unsigned long)descriptor->ep_api_version,
                (unsigned long)expected_api_version);
        }
        flipper_application_free(application);
        return NULL;
    }

    instance->application = application;
    instance->descriptor = descriptor;
#if FLIPPASS_ENABLE_LOGS
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free = memmgr_heap_get_max_free_block();
    FLIPPASS_LOG_EVENT(
        app,
        "MODULE_LOAD slot=%s appid=%s path=%s free=%lu max=%lu",
        flippass_module_slot_name(slot),
        descriptor->appid,
        module_path,
        (unsigned long)free_heap,
        (unsigned long)max_free);
#endif
    return instance->descriptor;
}
