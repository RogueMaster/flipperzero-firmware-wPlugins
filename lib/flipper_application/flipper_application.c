#include "flipper_application.h"
#include "elf/elf_file.h"
#include <notification/notification_messages.h>
#include "application_assets.h"
#include "application_metadata.h"
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage_processing.h>
#ifndef FURI_RAM_EXEC
#include <mjs/fal/mjs_fal_imports.h>
#endif

#include <m-list.h>

#define TAG "Fap"

struct FlipperApplication {
    ELFDebugInfo state;
    FlipperApplicationManifest manifest;
    ELFFile* elf;
#ifndef FURI_RAM_EXEC
    MjsFalImports mjs_imports;
#endif
    FuriThread* thread;
    void* ep_thread_args;

    bool preloaded_manifest;
    FlipperApplicationAssetsProgress assets_progress;
    void* assets_progress_context;
};

/********************** Debugger access to loader state **********************/

LIST_DEF(FlipperApplicationList, const FlipperApplication*, M_POD_OPLIST); // NOLINT

FlipperApplicationList_t flipper_application_loaded_app_list = {0};
static bool flipper_application_loaded_app_list_initialized = false;

static void flipper_application_list_add_app(const FlipperApplication* app) {
    furi_check(app);

    if(!flipper_application_loaded_app_list_initialized) {
        FlipperApplicationList_init(flipper_application_loaded_app_list);
        flipper_application_loaded_app_list_initialized = true;
    }
    FlipperApplicationList_push_back(flipper_application_loaded_app_list, app);
}

static void flipper_application_list_remove_app(const FlipperApplication* app) {
    furi_check(flipper_application_loaded_app_list_initialized);
    furi_check(app);

    FlipperApplicationList_it_t it;
    for(FlipperApplicationList_it(it, flipper_application_loaded_app_list);
        !FlipperApplicationList_end_p(it);
        FlipperApplicationList_next(it)) {
        if(*FlipperApplicationList_ref(it) == app) {
            FlipperApplicationList_remove(flipper_application_loaded_app_list, it);
            break;
        }
    }
}

/*****************************************************************************/

FlipperApplication*
    flipper_application_alloc(Storage* storage, const ElfApiInterface* api_interface) {
    furi_check(storage);
    furi_check(api_interface);

    FlipperApplication* app = malloc(sizeof(FlipperApplication));
#ifndef FURI_RAM_EXEC
    mjs_fal_imports_init(&app->mjs_imports, api_interface);
    api_interface = &app->mjs_imports.api;
#endif
    app->elf = elf_file_alloc(storage, api_interface);
    app->thread = NULL;
    app->ep_thread_args = NULL;
    app->assets_progress = NULL;
    app->assets_progress_context = NULL;

    return app;
}

bool flipper_application_is_plugin(FlipperApplication* app) {
    furi_check(app);
    return app->manifest.stack_size == 0;
}

void flipper_application_free(FlipperApplication* app) {
    furi_check(app);

    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
    }

    if(app->state.entry) {
        flipper_application_list_remove_app(app);
    }

    elf_file_clear_debug_info(&app->state);

    if(elf_file_is_init_complete(app->elf)) {
        elf_file_call_fini(app->elf);
    }

    elf_file_free(app->elf);
#ifndef FURI_RAM_EXEC
    /* Keep the engine available through ELF destructors and module cleanup. */
    mjs_fal_imports_release(&app->mjs_imports);
#endif

    if(app->ep_thread_args) {
        free(app->ep_thread_args);
        app->ep_thread_args = NULL;
    }

    free(app);
}

static FlipperApplicationPreloadStatus
    flipper_application_validate_manifest(FlipperApplication* app) {
    if(!flipper_application_manifest_is_valid(&app->manifest)) {
        return FlipperApplicationPreloadStatusInvalidManifest;
    }

    if(!flipper_application_manifest_is_target_compatible(&app->manifest)) {
        return FlipperApplicationPreloadStatusTargetMismatch;
    }

    if(!flipper_application_manifest_is_too_old(
           &app->manifest, elf_file_get_api_interface(app->elf))) {
        return FlipperApplicationPreloadStatusApiTooOld;
    }

    if(!flipper_application_manifest_is_too_new(
           &app->manifest, elf_file_get_api_interface(app->elf))) {
        return FlipperApplicationPreloadStatusApiTooNew;
    }

    return FlipperApplicationPreloadStatusSuccess;
}

static bool flipper_application_process_manifest_section(
    File* file,
    size_t offset,
    size_t size,
    void* context) {
    FlipperApplicationManifest* manifest = context;

    // Support both OFW manifest and extended manifest with flags
    if(size < sizeof(FlipperApplicationManifestOfw) ||
       size > sizeof(FlipperApplicationManifestEx)) {
        return false;
    }

    if(manifest == NULL) {
        return true;
    }

    bool result = storage_file_seek(file, offset, true) &&
                  storage_file_read(file, manifest, size) == size;

    // Default flags when loading OFW manifests that don't include flags
    if(result && size < sizeof(FlipperApplicationManifestEx)) {
        manifest->flags = FlipperApplicationFlagDefault;
    }

    return result;
}

// we can't use const char* as context because we will lose the const qualifier
typedef struct {
    const char* path;
    FlipperApplicationAssetsProgress progress;
    void* progress_context;
} FlipperApplicationPreloadAssetsContext;

static bool flipper_application_process_assets_section(
    File* file,
    size_t offset,
    size_t size,
    void* context) {
    FlipperApplicationPreloadAssetsContext* preload_context = context;
    return flipper_application_assets_load(
        file,
        preload_context->path,
        offset,
        size,
        preload_context->progress,
        preload_context->progress_context);
}

static FlipperApplicationPreloadStatus
    flipper_application_load(FlipperApplication* app, const char* path, bool load_full) {
    if(!app->preloaded_manifest && !elf_file_open(app->elf, path)) {
        return FlipperApplicationPreloadStatusInvalidFile;
    }

    // if we are loading full file
    if(load_full) {
        // load section table
        ElfLoadSectionTableResult load_result = elf_file_load_section_table(app->elf);
        if(load_result == ElfLoadSectionTableResultError) {
            return FlipperApplicationPreloadStatusInvalidFile;
        } else if(load_result == ElfLoadSectionTableResultNoMemory) {
            return FlipperApplicationPreloadStatusNotEnoughMemory;
        }

        // load assets section
        FlipperApplicationPreloadAssetsContext preload_context = {
            .path = path,
            .progress = app->assets_progress,
            .progress_context = app->assets_progress_context,
        };
        if(elf_process_section(
               app->elf,
               ".fapassets",
               flipper_application_process_assets_section,
               &preload_context) == ElfProcessSectionResultCannotProcess) {
            return FlipperApplicationPreloadStatusInvalidFile;
        }
    }

    // load manifest section
    if(!app->preloaded_manifest &&
       elf_process_section(
           app->elf, ".fapmeta", flipper_application_process_manifest_section, &app->manifest) !=
           ElfProcessSectionResultSuccess) {
        return FlipperApplicationPreloadStatusInvalidFile;
    }

    // Avoid preloading manifest twice, when user calls both preload_manifest() and preload()
    if(!load_full) {
        app->preloaded_manifest = true;
    }

    return flipper_application_validate_manifest(app);
}

void flipper_application_set_assets_progress_callback(
    FlipperApplication* app,
    FlipperApplicationAssetsProgress callback,
    void* context) {
    furi_check(app);
    app->assets_progress = callback;
    app->assets_progress_context = context;
}

/* Parse headers, load manifest */
FlipperApplicationPreloadStatus
    flipper_application_preload_manifest(FlipperApplication* app, const char* path) {
    furi_check(app);
    furi_check(path);

    return flipper_application_load(app, path, false);
}

/* Parse headers, load full file */
FlipperApplicationPreloadStatus
    flipper_application_preload(FlipperApplication* app, const char* path) {
    furi_check(app);
    furi_check(path);

    return flipper_application_load(app, path, true);
}

const FlipperApplicationManifest* flipper_application_get_manifest(FlipperApplication* app) {
    furi_check(app);
    return &app->manifest;
}

FlipperApplicationLoadStatus flipper_application_map_to_memory(FlipperApplication* app) {
    furi_check(app);

    ELFFileLoadStatus status = elf_file_load_sections(app->elf);
#ifndef FURI_RAM_EXEC
    if(app->mjs_imports.failed) return FlipperApplicationLoadStatusMissingRuntime;
#endif

    switch(status) {
    case ELFFileLoadStatusSuccess:
        elf_file_init_debug_info(app->elf, &app->state);
        flipper_application_list_add_app(app);
        return FlipperApplicationLoadStatusSuccess;
    case ELFFileLoadStatusMissingImports:
        return FlipperApplicationLoadStatusMissingImports;
    default:
        return FlipperApplicationLoadStatusUnspecifiedError;
    }
}

static int32_t flipper_application_thread(void* context) {
    furi_check(context);
    FlipperApplication* app = (FlipperApplication*)context;

    elf_file_call_init(app->elf);

    FlipperApplicationEntryPoint entry_point = elf_file_get_entry_point(app->elf);
    int32_t ret_code = entry_point(app->ep_thread_args);

    elf_file_call_fini(app->elf);

    // wait until all notifications from RAM are completed
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message_block(notifications, &sequence_empty);
    furi_record_close(RECORD_NOTIFICATION);

    return ret_code;
}

FuriThread* flipper_application_alloc_thread(FlipperApplication* app, const char* args) {
    furi_check(app);
    furi_check(app->thread == NULL);
    furi_check(!flipper_application_is_plugin(app));

    if(app->ep_thread_args) {
        free(app->ep_thread_args);
    }

    if(args) {
        app->ep_thread_args = strdup(args);
    } else {
        app->ep_thread_args = NULL;
    }

    const FlipperApplicationManifest* manifest = flipper_application_get_manifest(app);
    app->thread = furi_thread_alloc_ex(
        manifest->name, manifest->stack_size, flipper_application_thread, app);

    return app->thread;
}

const char* flipper_application_preload_status_to_string(FlipperApplicationPreloadStatus status) {
    switch(status) {
    case FlipperApplicationPreloadStatusSuccess:
        return "Success";
    case FlipperApplicationPreloadStatusInvalidFile:
        return "Invalid file";
    case FlipperApplicationPreloadStatusNotEnoughMemory:
        return "Not enough memory";
    case FlipperApplicationPreloadStatusInvalidManifest:
        return "Invalid file manifest";
    case FlipperApplicationPreloadStatusApiTooOld:
        return "Update Application to use with this Firmware (ApiTooOld)";
    case FlipperApplicationPreloadStatusApiTooNew:
        return "Update Firmware to use with this Application (ApiTooNew)";
    case FlipperApplicationPreloadStatusTargetMismatch:
        return "Hardware target mismatch";
    }

    return "Unknown error";
}

const char* flipper_application_load_status_to_string(FlipperApplicationLoadStatus status) {
    switch(status) {
    case FlipperApplicationLoadStatusSuccess:
        return "Success";
    case FlipperApplicationLoadStatusUnspecifiedError:
        return "Unknown error";
    case FlipperApplicationLoadStatusMissingImports:
        return "Update Application or Firmware to compatible versions (MissingImports)";
    case FlipperApplicationLoadStatusMissingRuntime:
        return "mJS engine unavailable. Check matching SD resources (mjs_engine.fal) and free memory";
    }

    return "Unknown error";
}

const FlipperAppPluginDescriptor*
    flipper_application_plugin_get_descriptor(FlipperApplication* app) {
    furi_check(app);

    if(!flipper_application_is_plugin(app)) {
        return NULL;
    }

    if(!elf_file_is_init_complete(app->elf)) {
        elf_file_call_init(app->elf);
    }

    typedef const FlipperAppPluginDescriptor* (*get_lib_descriptor_t)(void);
    get_lib_descriptor_t lib_ep = elf_file_get_entry_point(app->elf);
    furi_check(lib_ep);

    const FlipperAppPluginDescriptor* lib_descriptor = lib_ep();

    FURI_LOG_D(
        TAG,
        "Library for %s, API v. %lu loaded",
        lib_descriptor->appid,
        lib_descriptor->ep_api_version);

    return lib_descriptor;
}

bool flipper_application_load_name_and_icon(
    FuriString* path,
    Storage* storage,
    uint8_t** icon_ptr,
    FuriString* item_name) {
    furi_check(path);
    furi_check(storage);
    furi_check(icon_ptr);
    furi_check(item_name);

    bool load_success = true;

    StorageData* storage_data;
    if(storage_get_data(storage, path, &storage_data) == FSE_OK &&
       storage_path_already_open(path, storage_data)) {
        load_success = false;
    }

    if(load_success) {
        load_success = false;

        FlipperApplicationManifest manifest;
        if(flipper_application_metadata_load(storage, furi_string_get_cstr(path), &manifest)) {
            if(manifest.has_icon) {
                memcpy(*icon_ptr, manifest.icon, FAP_MANIFEST_MAX_ICON_SIZE);
            } else {
                // Raw 10x10 file glyph, padded to the manifest icon buffer size.
                // A valid icon-less FAP still keeps its manifest display name.
                static const uint8_t fallback_icon[FAP_MANIFEST_MAX_ICON_SIZE] = {
                    0x00, 0x7F, 0x00, 0xA1, 0x00, 0x2D, 0x01, 0xE1, 0x01, 0x0D, 0x01,
                    0x01, 0x01, 0x7D, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF, 0x01,
                };
                memcpy(*icon_ptr, fallback_icon, sizeof(fallback_icon));
            }
            furi_string_set(item_name, manifest.name);
            load_success = true;
        } else {
            FURI_LOG_E(TAG, "Failed to preload %s", furi_string_get_cstr(path));
            load_success = false;
        }
    }

    if(!load_success) {
        size_t offset = furi_string_search_rchar(path, '/');
        if(offset != FURI_STRING_FAILURE) {
            furi_string_set_n(item_name, path, offset + 1, furi_string_size(path) - offset - 1);
        } else {
            furi_string_set(item_name, path);
        }
    }

    return load_success;
}
