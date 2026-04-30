#include "flippass.h"

#include <loader/firmware_api/firmware_api.h>

#include <string.h>

#define FLIPPASS_OUTPUT_USB_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_output_usb.fal")
#define FLIPPASS_OUTPUT_BLE_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_output_ble.fal")
#define FLIPPASS_OUTPUT_ACTION_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_output_action.fal")
#define FLIPPASS_OTHER_FIELDS_PLUGIN_PATH  APP_ASSETS_PATH("plugins/flippass_other_fields.fal")
#define FLIPPASS_FILE_OPS_PLUGIN_PATH      APP_ASSETS_PATH("plugins/flippass_file_ops.fal")
#define FLIPPASS_EDITOR_CRUD_PLUGIN_PATH   APP_ASSETS_PATH("plugins/flippass_editor_crud.fal")
#define FLIPPASS_RPC_COMMANDS_PLUGIN_PATH  APP_ASSETS_PATH("plugins/flippass_rpc_commands.fal")
#define FLIPPASS_OPEN_ACQUIRE_PLUGIN_PATH  APP_ASSETS_PATH("plugins/flippass_open_acquire.fal")
#define FLIPPASS_OPEN_STREAM_PLUGIN_PATH   APP_ASSETS_PATH("plugins/flippass_open_stream.fal")
#define FLIPPASS_OPEN_INFLATE_NONPAGED_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_open_inflate_nonpaged.fal")
#define FLIPPASS_OPEN_INFLATE_PAGED_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_open_inflate_paged.fal")
#define FLIPPASS_OPEN_MODEL_PLUGIN_PATH  APP_ASSETS_PATH("plugins/flippass_open_model.fal")
#define FLIPPASS_SAVE_HEADER_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_save_header.fal")
#define FLIPPASS_SAVE_WRITER_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_save_writer.fal")
#define FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_keyboard_layout.fal")
#define FLIPPASS_OTP_PLUGIN_PATH          APP_ASSETS_PATH("plugins/flippass_otp.fal")
#define FLIPPASS_PASSWORD_GEN_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_password_gen.fal")

const char* flippass_module_slot_name(FlipPassModuleSlot slot) {
    switch(slot) {
    case FlipPassModuleSlotOutputUsb:
        return "output_usb";
    case FlipPassModuleSlotOutputBle:
        return "output_ble";
    case FlipPassModuleSlotOutputAction:
        return "output_action";
    case FlipPassModuleSlotOtherFields:
        return "other_fields";
    case FlipPassModuleSlotFileOps:
        return "file_ops";
    case FlipPassModuleSlotEditorCrud:
        return "editor_crud";
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
    case FlipPassModuleSlotSaveHeader:
        return "save_header";
    case FlipPassModuleSlotSaveWriter:
        return "save_writer";
    case FlipPassModuleSlotKeyboardLayout:
        return "keyboard_layout";
    case FlipPassModuleSlotOtp:
        return "otp";
    case FlipPassModuleSlotPasswordGen:
        return "password_gen";
    case FlipPassModuleSlotCount:
    default:
        return "unknown";
    }
}

static const char* flippass_module_default_path(FlipPassModuleSlot slot) {
    switch(slot) {
    case FlipPassModuleSlotOutputUsb:
        return FLIPPASS_OUTPUT_USB_PLUGIN_PATH;
    case FlipPassModuleSlotOutputBle:
        return FLIPPASS_OUTPUT_BLE_PLUGIN_PATH;
    case FlipPassModuleSlotOutputAction:
        return FLIPPASS_OUTPUT_ACTION_PLUGIN_PATH;
    case FlipPassModuleSlotOtherFields:
        return FLIPPASS_OTHER_FIELDS_PLUGIN_PATH;
    case FlipPassModuleSlotFileOps:
        return FLIPPASS_FILE_OPS_PLUGIN_PATH;
    case FlipPassModuleSlotEditorCrud:
        return FLIPPASS_EDITOR_CRUD_PLUGIN_PATH;
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
    case FlipPassModuleSlotSaveHeader:
        return FLIPPASS_SAVE_HEADER_PLUGIN_PATH;
    case FlipPassModuleSlotSaveWriter:
        return FLIPPASS_SAVE_WRITER_PLUGIN_PATH;
    case FlipPassModuleSlotKeyboardLayout:
        return FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_PATH;
    case FlipPassModuleSlotOtp:
        return FLIPPASS_OTP_PLUGIN_PATH;
    case FlipPassModuleSlotPasswordGen:
        return FLIPPASS_PASSWORD_GEN_PLUGIN_PATH;
    case FlipPassModuleSlotCount:
    default:
        return NULL;
    }
}

#if FLIPPASS_ENABLE_LOGS
static void flippass_module_log_load_fail(
    App* app,
    FlipPassModuleSlot slot,
    const char* phase,
    const char* path,
    const char* detail) {
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free = memmgr_heap_get_max_free_block();
    FLIPPASS_LOG_EVENT(
        app,
        "MODULE_LOAD_FAIL slot=%s phase=%s path=%s detail=%s free=%lu max=%lu",
        flippass_module_slot_name(slot),
        phase != NULL ? phase : "unknown",
        path != NULL ? path : "none",
        detail != NULL ? detail : "none",
        (unsigned long)free_heap,
        (unsigned long)max_free);
}
#endif

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
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_unload_before", slot, 0U);
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
        instance->application = NULL;
        instance->descriptor = NULL;
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_unload_after", slot, 0U);
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

    FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_begin", slot, 0U);

    if(app->module_loader.storage == NULL) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "storage", NULL, "unavailable");
#endif
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass plugin storage is unavailable.");
        }
        return NULL;
    }

    const char* module_path = (path != NULL) ? path : flippass_module_default_path(slot);
    if(module_path == NULL) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "path", NULL, "undefined");
#endif
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass plugin path is not defined.");
        }
        return NULL;
    }

    FlipperApplication* application =
        flipper_application_alloc(app->module_loader.storage, firmware_api_interface);
    if(application == NULL) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "alloc", module_path, "null");
#endif
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not allocate the requested plugin.");
        }
        return NULL;
    }
    FLIPPASS_MEMORY_LOG_MODULE(app, "module_alloc_ok", slot, 0U);

    const FlipperApplicationPreloadStatus preload_status =
        flipper_application_preload(application, module_path);
    if(preload_status != FlipperApplicationPreloadStatusSuccess) {
        const char* preload_status_text =
            flipper_application_preload_status_to_string(preload_status);
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "preload", module_path, preload_status_text);
#endif
        if(error != NULL) {
            furi_string_printf(
                error, "FlipPass could not preload %s (%s).", module_path, preload_status_text);
        }
        flipper_application_free(application);
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
        return NULL;
    }
    FLIPPASS_MEMORY_LOG_MODULE(app, "module_preload_ok", slot, 0U);

    if(!flipper_application_is_plugin(application)) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "plugin_check", module_path, "not_plugin");
#endif
        if(error != NULL) {
            furi_string_printf(error, "%s is not a FlipPass plugin.", module_path);
        }
        flipper_application_free(application);
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
        return NULL;
    }

    FLIPPASS_MEMORY_LOG_MODULE(app, "module_map_begin", slot, 0U);
    const FlipperApplicationLoadStatus load_status =
        flipper_application_map_to_memory(application);
    if(load_status != FlipperApplicationLoadStatusSuccess) {
        const char* load_status_text = flipper_application_load_status_to_string(load_status);
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "map", module_path, load_status_text);
#endif
        if(error != NULL) {
            furi_string_printf(
                error, "FlipPass could not map %s (%s).", module_path, load_status_text);
        }
        flipper_application_free(application);
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
        return NULL;
    }
    FLIPPASS_MEMORY_LOG_MODULE(app, "module_map_ok", slot, 0U);

    const FlipperAppPluginDescriptor* descriptor =
        flipper_application_plugin_get_descriptor(application);
    if(descriptor == NULL) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "descriptor", module_path, "null");
#endif
        if(error != NULL) {
            furi_string_printf(error, "%s did not expose a plugin descriptor.", module_path);
        }
        flipper_application_free(application);
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
        return NULL;
    }

    if(expected_appid != NULL && strcmp(descriptor->appid, expected_appid) != 0) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "appid", module_path, descriptor->appid);
#endif
        if(error != NULL) {
            furi_string_printf(
                error,
                "FlipPass loaded %s but received plugin appid %s.",
                module_path,
                descriptor->appid);
        }
        flipper_application_free(application);
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
        return NULL;
    }

    if(expected_api_version != 0U && descriptor->ep_api_version != expected_api_version) {
#if FLIPPASS_ENABLE_LOGS
        flippass_module_log_load_fail(app, slot, "api", module_path, descriptor->appid);
#endif
        if(error != NULL) {
            furi_string_printf(
                error,
                "FlipPass plugin %s uses API %lu instead of %lu.",
                descriptor->appid,
                (unsigned long)descriptor->ep_api_version,
                (unsigned long)expected_api_version);
        }
        flipper_application_free(application);
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
        return NULL;
    }

    instance->application = application;
    instance->descriptor = descriptor;
    FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_ready", slot, 0U);
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
