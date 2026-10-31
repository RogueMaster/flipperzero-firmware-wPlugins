#include "flippass.h"
#include "flippass_db.h"
#include "flippass_app_api.h"
#include "scenes/flippass_scene_editor.h"

#include <core/kernel.h>
#include <elf.h>
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>

#include <stdint.h>
#include <string.h>

/* Plugin Last escrow: held from just before flipper_application_alloc to just
 * after flipper_application_map_to_memory so low-address fitting blocks stay
 * unavailable to the loader while a later block is used for plugin sections.
 * On unload, mapped sections are more likely to coalesce back into a useful
 * tail block.
 *
 * Sizing starts from the loader-visible ELF sections and the on-disk .fal size.
 * The section floor mirrors flipper_application_preload(): each loaded section
 * needs section-size + 1024 bytes of largest-free-block headroom. The .fal size
 * is used as an additional placement floor, and a 90% .fal-sized slack is kept
 * available during map so relocation and init-time allocations are not starved.
 *
 * The placement loop is a bounded Data-Fit/Plugin-Last hybrid. It temporarily
 * reserves whole oversized blocks with safe requests, leaves lower-address
 * holes below the plugin load floor, frees one later fitting block as the
 * plugin target, then releases all escrows immediately after map succeeds or
 * on every failure path. */
#define FLIPPASS_PLUGIN_LAST_SECTION_GUARD_BYTES      1024U
#define FLIPPASS_PLUGIN_LAST_METADATA_GUARD_BYTES     1024U
#define FLIPPASS_PLUGIN_LAST_ALLOC_GUARD_BYTES        1024U
#define FLIPPASS_PLUGIN_LAST_BASE_LANE_BYTES          1024U
#define FLIPPASS_PLUGIN_LAST_INIT_SLACK_PERCENT       90U
#define FLIPPASS_PLUGIN_LAST_MIN_ESCROW_REQUEST_BYTES 64U
#define FLIPPASS_PLUGIN_LAST_MAX_ESCROWS              6U
#define FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS       3U
#define FLIPPASS_MODULE_LOAD_ATTEMPTS                 (FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS + 1U)

#define FLIPPASS_OUTPUT_USB_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_output_usb.fal")
#define FLIPPASS_OUTPUT_BLE_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_output_ble.fal")
#define FLIPPASS_OUTPUT_ACTION_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_output_action.fal")
#define FLIPPASS_ENTRY_ACTION_PLUGIN_PATH  APP_ASSETS_PATH("plugins/flippass_entry_action.fal")
#define FLIPPASS_TEXT_INPUT_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_text_input.fal")
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
#define FLIPPASS_SAVE_SERIAL_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_save_serial.fal")
#define FLIPPASS_SAVE_WRITER_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_save_writer.fal")
#define FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/flippass_keyboard_layout.fal")
#define FLIPPASS_OTP_PLUGIN_PATH          APP_ASSETS_PATH("plugins/flippass_otp.fal")
#define FLIPPASS_PASSWORD_GEN_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_password_gen.fal")
#define FLIPPASS_CRYPTO_PLUGIN_PATH       APP_ASSETS_PATH("plugins/flippass_crypto.fal")
#define FLIPPASS_OPEN_HOST_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_open_host.fal")
#define FLIPPASS_EDITOR_PLUGIN_PATH       APP_ASSETS_PATH("plugins/flippass_editor.fal")
#define FLIPPASS_SAVE_HOST_PLUGIN_PATH    APP_ASSETS_PATH("plugins/flippass_save_host.fal")
#define FLIPPASS_FILE_BROWSER_PLUGIN_PATH APP_ASSETS_PATH("plugins/flippass_file_browser.fal")
#define FLIPPASS_LAUNCH_PLUGIN_PATH       APP_ASSETS_PATH("plugins/flippass_launch.fal")

const char* flippass_module_slot_name(FlipPassModuleSlot slot) {
    switch(slot) {
    case FlipPassModuleSlotOutputUsb:
        return "output_usb";
    case FlipPassModuleSlotOutputBle:
        return "output_ble";
    case FlipPassModuleSlotOutputAction:
        return "output_action";
    case FlipPassModuleSlotEntryAction:
        return "entry_action";
    case FlipPassModuleSlotTextInput:
        return "text_input";
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
    case FlipPassModuleSlotSaveSerial:
        return "save_serial";
    case FlipPassModuleSlotSaveWriter:
        return "save_writer";
    case FlipPassModuleSlotKeyboardLayout:
        return "keyboard_layout";
    case FlipPassModuleSlotOtp:
        return "otp";
    case FlipPassModuleSlotPasswordGen:
        return "password_gen";
    case FlipPassModuleSlotCrypto:
        return "crypto";
    case FlipPassModuleSlotOpenHost:
        return "open_host";
    case FlipPassModuleSlotEditor:
        return "editor";
    case FlipPassModuleSlotSaveHost:
        return "save_host";
    case FlipPassModuleSlotFileBrowser:
        return "file_browser";
    case FlipPassModuleSlotLaunch:
        return "launch";
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
    case FlipPassModuleSlotEntryAction:
        return FLIPPASS_ENTRY_ACTION_PLUGIN_PATH;
    case FlipPassModuleSlotTextInput:
        return FLIPPASS_TEXT_INPUT_PLUGIN_PATH;
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
    case FlipPassModuleSlotSaveSerial:
        return FLIPPASS_SAVE_SERIAL_PLUGIN_PATH;
    case FlipPassModuleSlotSaveWriter:
        return FLIPPASS_SAVE_WRITER_PLUGIN_PATH;
    case FlipPassModuleSlotKeyboardLayout:
        return FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_PATH;
    case FlipPassModuleSlotOtp:
        return FLIPPASS_OTP_PLUGIN_PATH;
    case FlipPassModuleSlotPasswordGen:
        return FLIPPASS_PASSWORD_GEN_PLUGIN_PATH;
    case FlipPassModuleSlotCrypto:
        return FLIPPASS_CRYPTO_PLUGIN_PATH;
    case FlipPassModuleSlotOpenHost:
        return FLIPPASS_OPEN_HOST_PLUGIN_PATH;
    case FlipPassModuleSlotEditor:
        return FLIPPASS_EDITOR_PLUGIN_PATH;
    case FlipPassModuleSlotSaveHost:
        return FLIPPASS_SAVE_HOST_PLUGIN_PATH;
    case FlipPassModuleSlotFileBrowser:
        return FLIPPASS_FILE_BROWSER_PLUGIN_PATH;
    case FlipPassModuleSlotLaunch:
        return FLIPPASS_LAUNCH_PLUGIN_PATH;
    case FlipPassModuleSlotCount:
    default:
        return NULL;
    }
}

static const char* flippass_module_path_basename(const char* path) {
    const char* basename = path;

    if(path == NULL) {
        return "plugin";
    }

    for(const char* cursor = path; *cursor != '\0'; cursor++) {
        if(*cursor == '/' || *cursor == '\\') {
            basename = cursor + 1;
        }
    }

    return basename[0] != '\0' ? basename : path;
}

#if FLIPPASS_EVENT_LOG_ENABLED
static void flippass_module_log_load_fail(
    App* app,
    FlipPassModuleSlot slot,
    const char* phase,
    const char* path,
    const char* detail) {
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free = memmgr_heap_get_max_free_block();
    FLIPPASS_PROBE_LOG_EVENT(
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

static bool
    flippass_module_retry_preserve_slot(FlipPassModuleSlot target, FlipPassModuleSlot loaded) {
    if(loaded == target) {
        return true;
    }

    /* Output transports own external HID state; keep their explicit cleanup
     * path in charge instead of freeing them from a generic loader retry. */
    if(loaded == FlipPassModuleSlotOutputUsb || loaded == FlipPassModuleSlotOutputBle) {
        return true;
    }

    /* Text input and the active picker retain GUI callbacks between calls. */
    if(loaded == FlipPassModuleSlotEntryAction || loaded == FlipPassModuleSlotTextInput ||
       loaded == FlipPassModuleSlotCrypto || loaded == FlipPassModuleSlotFileBrowser) {
        return true;
    }

    if(target == FlipPassModuleSlotOutputAction &&
       (loaded == FlipPassModuleSlotOutputUsb || loaded == FlipPassModuleSlotOutputBle)) {
        return true;
    }

    return false;
}

static bool flippass_module_retry_can_clear_title_cache(App* app, FlipPassModuleSlot target) {
    /* OutputAction receives request pointers prepared immediately before load;
     * do not evict the active entry title after those pointers are captured. */
    return target != FlipPassModuleSlotOutputAction && target != FlipPassModuleSlotEntryAction &&
           app->module_loader.slot[FlipPassModuleSlotOutputAction].pin_count == 0U &&
           app->module_loader.slot[FlipPassModuleSlotEntryAction].pin_count == 0U;
}

static bool flippass_module_prepare_memory_retry(
    App* app,
    FlipPassModuleSlot target,
    const char* phase,
    const char* path,
    const char* detail) {
    bool released = false;

    furi_assert(app);

#if !FLIPPASS_ENABLE_LOGS
    UNUSED(phase);
    UNUSED(path);
    UNUSED(detail);
#endif

    FLIPPASS_MEMORY_LOG_MODULE(app, "module_retry_trim_begin", target, 0U);
#if FLIPPASS_ENABLE_LOGS
    FLIPPASS_LOG_EVENT(
        app,
        "MODULE_LOAD_RETRY slot=%s phase=%s path=%s detail=%s free=%lu max=%lu",
        flippass_module_slot_name(target),
        phase != NULL ? phase : "unknown",
        path != NULL ? path : "none",
        detail != NULL ? detail : "none",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
#endif

    if(flippass_module_retry_can_clear_title_cache(app, target)) {
        released = flippass_db_browser_clear_title_cache(app);
    }

    for(size_t index = 0U; index < FlipPassModuleSlotCount; index++) {
        const FlipPassModuleSlot loaded = (FlipPassModuleSlot)index;
        if(flippass_module_retry_preserve_slot(target, loaded) ||
           app->module_loader.slot[index].pin_count != 0U ||
           (loaded == FlipPassModuleSlotEditor && flippass_editor_in_use(app))) {
            continue;
        }
        if(app->module_loader.slot[index].application == NULL) {
            continue;
        }
#if FLIPPASS_ENABLE_LOGS
        FLIPPASS_LOG_EVENT(
            app,
            "MODULE_RETRY_UNLOAD target=%s loaded=%s",
            flippass_module_slot_name(target),
            flippass_module_slot_name(loaded));
#endif
        flippass_module_unload(app, loaded);
        released |= app->module_loader.slot[index].application == NULL;
    }

    FLIPPASS_MEMORY_LOG_MODULE(app, "module_retry_trim_after", target, 0U);
    return released;
}

typedef struct {
    void* ptr[FLIPPASS_PLUGIN_LAST_MAX_ESCROWS];
    size_t size[FLIPPASS_PLUGIN_LAST_MAX_ESCROWS];
    size_t max_before[FLIPPASS_PLUGIN_LAST_MAX_ESCROWS];
    size_t max_after[FLIPPASS_PLUGIN_LAST_MAX_ESCROWS];
    size_t free_after[FLIPPASS_PLUGIN_LAST_MAX_ESCROWS];
    uintptr_t address[FLIPPASS_PLUGIN_LAST_MAX_ESCROWS];
    size_t load_floor;
    size_t placement_floor;
    size_t init_slack;
    size_t lane;
    size_t file_size;
    uint8_t attempt;
    uint8_t count;
} FlipPassPluginLastEscrow;

typedef struct {
    size_t load_floor;
    size_t largest_section_floor;
    size_t file_size;
    uint32_t loaded_section_count;
} FlipPassPluginLastProfile;

static bool flippass_module_plugin_last_name_has_prefix(
    File* file,
    const Elf32_Shdr* shstr,
    uint32_t name_offset,
    const char* prefix) {
    char c = '\0';
    size_t offset = 0U;

    if(file == NULL || shstr == NULL || prefix == NULL || name_offset >= shstr->sh_size) {
        return false;
    }

    while(prefix[offset] != '\0') {
        if(name_offset + offset >= shstr->sh_size ||
           !storage_file_seek(file, shstr->sh_offset + name_offset + offset, true) ||
           storage_file_read(file, &c, 1U) != 1U || c != prefix[offset]) {
            return false;
        }
        offset++;
    }

    return true;
}

static bool flippass_module_plugin_last_section_ignored(
    File* file,
    const Elf32_Shdr* shstr,
    const Elf32_Shdr* section) {
    return flippass_module_plugin_last_name_has_prefix(file, shstr, section->sh_name, ".ARM.") ||
           flippass_module_plugin_last_name_has_prefix(
               file, shstr, section->sh_name, ".rel.ARM.") ||
           flippass_module_plugin_last_name_has_prefix(
               file, shstr, section->sh_name, ".fast.rel.ARM.");
}

static bool flippass_module_plugin_last_section_fast_rel(
    File* file,
    const Elf32_Shdr* shstr,
    const Elf32_Shdr* section) {
    return flippass_module_plugin_last_name_has_prefix(file, shstr, section->sh_name, ".fast.rel");
}

static bool flippass_module_plugin_last_profile(
    App* app,
    Storage* storage,
    FlipPassModuleSlot slot,
    const char* path,
    FlipPassPluginLastProfile* out_profile) {
    File* file = NULL;
    Elf32_Ehdr header;
    Elf32_Shdr shstr;
    FlipPassPluginLastProfile profile;
    size_t load_floor = 0U;
    size_t largest_section_floor = 0U;
    uint32_t loaded_section_count = 0U;
    bool ok = false;
    bool parse_error = false;

    furi_assert(out_profile);
    memset(out_profile, 0, sizeof(*out_profile));
    memset(&profile, 0, sizeof(profile));

    if(storage == NULL || path == NULL) {
        return false;
    }

#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(app);
    UNUSED(slot);
#endif

    file = storage_file_alloc(storage);
    if(file == NULL) {
        return false;
    }

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) ||
           storage_file_read(file, &header, sizeof(header)) != sizeof(header) ||
           memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
           header.e_ident[EI_CLASS] != ELFCLASS32 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
           header.e_shentsize != sizeof(Elf32_Shdr) || header.e_shnum == 0U ||
           header.e_shstrndx == SHN_UNDEF || header.e_shstrndx >= header.e_shnum) {
            break;
        }

        const uint64_t file_size = storage_file_size(file);
        if(file_size > (uint64_t)SIZE_MAX) {
            parse_error = true;
            break;
        }
        profile.file_size = (size_t)file_size;

        if(!storage_file_seek(
               file, header.e_shoff + ((uint32_t)header.e_shstrndx * sizeof(Elf32_Shdr)), true) ||
           storage_file_read(file, &shstr, sizeof(shstr)) != sizeof(shstr)) {
            break;
        }

        for(uint16_t index = 1U; index < header.e_shnum; index++) {
            Elf32_Shdr section;
            if(!storage_file_seek(
                   file, header.e_shoff + ((uint32_t)index * sizeof(Elf32_Shdr)), true) ||
               storage_file_read(file, &section, sizeof(section)) != sizeof(section)) {
                load_floor = 0U;
                parse_error = true;
                break;
            }

            if(section.sh_size == 0U ||
               flippass_module_plugin_last_section_ignored(file, &shstr, &section)) {
                continue;
            }

            const bool loaded_section =
                (section.sh_flags & SHF_ALLOC) ||
                flippass_module_plugin_last_section_fast_rel(file, &shstr, &section);
            if(!loaded_section) {
                continue;
            }

            if(section.sh_size > (SIZE_MAX - FLIPPASS_PLUGIN_LAST_SECTION_GUARD_BYTES)) {
                load_floor = 0U;
                parse_error = true;
                break;
            }
            const size_t safe_size =
                (size_t)section.sh_size + FLIPPASS_PLUGIN_LAST_SECTION_GUARD_BYTES;
            if(safe_size > (SIZE_MAX - load_floor)) {
                load_floor = 0U;
                parse_error = true;
                break;
            }
            load_floor += safe_size;
            if(safe_size > largest_section_floor) {
                largest_section_floor = safe_size;
            }
            loaded_section_count++;
        }

        profile.load_floor = load_floor;
        profile.largest_section_floor = largest_section_floor;
        profile.loaded_section_count = loaded_section_count;
        ok = !parse_error && load_floor > 0U && profile.file_size > 0U;
    } while(false);

    storage_file_free(file);

    if(ok) {
        *out_profile = profile;
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "PLUGIN_LAST_PREFLIGHT slot=%s floor=%lu largest=%lu sections=%lu fal=%lu reserve_floor=%lu path=%s",
            flippass_module_slot_name(slot),
            (unsigned long)profile.load_floor,
            (unsigned long)profile.largest_section_floor,
            (unsigned long)profile.loaded_section_count,
            (unsigned long)profile.file_size,
            (unsigned long)(profile.load_floor + FLIPPASS_PLUGIN_LAST_METADATA_GUARD_BYTES),
            path);
    } else {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "PLUGIN_LAST_PREFLIGHT_SKIP slot=%s path=%s",
            flippass_module_slot_name(slot),
            path);
    }

    return ok;
}

static bool flippass_module_size_add(size_t lhs, size_t rhs, size_t* out) {
    furi_assert(out);
    if(lhs > (SIZE_MAX - rhs)) {
        *out = 0U;
        return false;
    }
    *out = lhs + rhs;
    return true;
}

static bool flippass_module_size_percent(size_t value, uint8_t percent, size_t* out) {
    furi_assert(out);
    if(percent == 0U) {
        *out = 0U;
        return true;
    }
    if(value > (SIZE_MAX / percent)) {
        *out = 0U;
        return false;
    }
    const size_t product = value * percent;
    *out = (product / 100U) + ((product % 100U) != 0U ? 1U : 0U);
    return true;
}

static size_t flippass_module_plugin_last_lane(uint8_t attempt) {
    size_t lane = FLIPPASS_PLUGIN_LAST_BASE_LANE_BYTES;
    for(uint8_t index = 0U; index < attempt; index++) {
        if(lane > (SIZE_MAX / 2U)) {
            return SIZE_MAX;
        }
        lane *= 2U;
    }
    return lane;
}

static bool flippass_module_plugin_last_compute_floors(
    const FlipPassPluginLastProfile* profile,
    uint8_t attempt,
    size_t* out_load_floor,
    size_t* out_placement_floor,
    size_t* out_init_slack,
    size_t* out_lane) {
    size_t lane = 0U;
    size_t section_floor = 0U;
    size_t fal_floor = 0U;
    size_t load_floor = 0U;
    size_t init_slack = 0U;
    size_t placement_floor = 0U;

    furi_assert(profile);
    furi_assert(out_load_floor);
    furi_assert(out_placement_floor);
    furi_assert(out_init_slack);
    furi_assert(out_lane);

    *out_load_floor = 0U;
    *out_placement_floor = 0U;
    *out_init_slack = 0U;
    *out_lane = 0U;

    if(profile->load_floor == 0U || profile->file_size == 0U) {
        return false;
    }

    lane = flippass_module_plugin_last_lane(attempt);
    if(lane == SIZE_MAX) {
        return false;
    }

    if(!flippass_module_size_add(
           profile->load_floor, FLIPPASS_PLUGIN_LAST_METADATA_GUARD_BYTES, &section_floor) ||
       !flippass_module_size_add(section_floor, lane, &section_floor) ||
       !flippass_module_size_add(profile->file_size, lane, &fal_floor) ||
       !flippass_module_size_percent(
           profile->file_size, FLIPPASS_PLUGIN_LAST_INIT_SLACK_PERCENT, &init_slack)) {
        return false;
    }

    load_floor = section_floor > fal_floor ? section_floor : fal_floor;
    if(!flippass_module_size_add(load_floor, init_slack, &placement_floor)) {
        return false;
    }

    *out_load_floor = load_floor;
    *out_placement_floor = placement_floor;
    *out_init_slack = init_slack;
    *out_lane = lane;
    return true;
}

static bool flippass_module_plugin_last_safe_request_from_max(
    size_t max_free,
    size_t preserve_floor,
    size_t* out_request) {
    size_t guarded_floor = 0U;

    furi_assert(out_request);
    *out_request = 0U;

    if(!flippass_module_size_add(
           preserve_floor, FLIPPASS_PLUGIN_LAST_ALLOC_GUARD_BYTES, &guarded_floor)) {
        return false;
    }

    if(max_free <= guarded_floor) {
        return false;
    }

    const size_t request = max_free - guarded_floor;
    if(request < FLIPPASS_PLUGIN_LAST_MIN_ESCROW_REQUEST_BYTES) {
        return false;
    }

    *out_request = request;
    return true;
}

static bool flippass_module_plugin_last_reserve(
    size_t preserve_floor,
    void** out_ptr,
    size_t* out_request,
    size_t* out_max_before,
    size_t* out_max_after) {
    void* ptr = NULL;
    size_t request = 0U;
    size_t max_before = 0U;
    size_t max_after = 0U;

    furi_assert(out_ptr);
    furi_assert(out_request);
    furi_assert(out_max_before);
    furi_assert(out_max_after);

    *out_ptr = NULL;
    *out_request = 0U;
    *out_max_before = 0U;
    *out_max_after = 0U;

    const int32_t lock = furi_kernel_lock();
    max_before = memmgr_heap_get_max_free_block();
    if(flippass_module_plugin_last_safe_request_from_max(max_before, preserve_floor, &request)) {
        ptr = malloc(request);
        max_after = memmgr_heap_get_max_free_block();
    }
    furi_kernel_restore_lock(lock);

    if(ptr == NULL) {
        return false;
    }

    *out_ptr = ptr;
    *out_request = request;
    *out_max_before = max_before;
    *out_max_after = max_after;
    return true;
}

static void flippass_module_plugin_last_store_escrow(
    FlipPassPluginLastEscrow* escrow,
    void* ptr,
    size_t request,
    size_t max_before,
    size_t max_after) {
    furi_assert(escrow);
    furi_assert(escrow->count < FLIPPASS_PLUGIN_LAST_MAX_ESCROWS);

    escrow->ptr[escrow->count] = ptr;
    escrow->size[escrow->count] = request;
    escrow->max_before[escrow->count] = max_before;
    escrow->max_after[escrow->count] = max_after;
    escrow->free_after[escrow->count] = memmgr_get_free_heap();
    escrow->address[escrow->count] = (uintptr_t)ptr;
    escrow->count++;
}

static void
    flippass_module_plugin_last_forget_escrow(FlipPassPluginLastEscrow* escrow, uint8_t index) {
    furi_assert(escrow);
    furi_assert(index < escrow->count);

    const uint8_t last = escrow->count - 1U;
    if(index != last) {
        escrow->ptr[index] = escrow->ptr[last];
        escrow->size[index] = escrow->size[last];
        escrow->max_before[index] = escrow->max_before[last];
        escrow->max_after[index] = escrow->max_after[last];
        escrow->free_after[index] = escrow->free_after[last];
        escrow->address[index] = escrow->address[last];
    }

    escrow->ptr[last] = NULL;
    escrow->size[last] = 0U;
    escrow->max_before[last] = 0U;
    escrow->max_after[last] = 0U;
    escrow->free_after[last] = 0U;
    escrow->address[last] = 0U;
    escrow->count--;
}

static bool flippass_module_plugin_last_forget_ptr(FlipPassPluginLastEscrow* escrow, void* ptr) {
    furi_assert(escrow);

    for(uint8_t index = 0U; index < escrow->count; index++) {
        if(escrow->ptr[index] == ptr) {
            flippass_module_plugin_last_forget_escrow(escrow, index);
            return true;
        }
    }

    return false;
}

static void flippass_module_plugin_last_release(
    App* app,
    FlipPassModuleSlot slot,
    FlipPassPluginLastEscrow* escrow,
    const char* phase) {
    if(escrow == NULL || escrow->count == 0U) {
        return;
    }

#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(app);
    UNUSED(slot);
    UNUSED(phase);
#endif

    while(escrow->count > 0U) {
        escrow->count--;
        const uint8_t index = escrow->count;
        const size_t released_size = escrow->size[escrow->count];
        const size_t acquire_max_before = escrow->max_before[index];
        const size_t acquire_max_after = escrow->max_after[index];
        const size_t acquire_free_after = escrow->free_after[index];
        const uintptr_t released_address = escrow->address[index];
#if !FLIPPASS_EVENT_LOG_ENABLED
        UNUSED(released_size);
        UNUSED(acquire_max_before);
        UNUSED(acquire_max_after);
        UNUSED(acquire_free_after);
        UNUSED(released_address);
#endif
        free(escrow->ptr[escrow->count]);
        escrow->ptr[escrow->count] = NULL;
        escrow->size[escrow->count] = 0U;
        escrow->max_before[escrow->count] = 0U;
        escrow->max_after[escrow->count] = 0U;
        escrow->free_after[escrow->count] = 0U;
        escrow->address[escrow->count] = 0U;

        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "PLUGIN_LAST_ESCROW_RELEASE slot=%s phase=%s attempt=%u index=%u size=%lu addr=%lu floor=%lu placement_floor=%lu init_slack=%lu lane=%lu fal=%lu acquire_max_before=%lu acquire_max_after=%lu acquire_free=%lu free=%lu max=%lu",
            flippass_module_slot_name(slot),
            phase != NULL ? phase : "unknown",
            escrow->attempt,
            index,
            (unsigned long)released_size,
            (unsigned long)released_address,
            (unsigned long)escrow->load_floor,
            (unsigned long)escrow->placement_floor,
            (unsigned long)escrow->init_slack,
            (unsigned long)escrow->lane,
            (unsigned long)escrow->file_size,
            (unsigned long)acquire_max_before,
            (unsigned long)acquire_max_after,
            (unsigned long)acquire_free_after,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }

    escrow->load_floor = 0U;
    escrow->placement_floor = 0U;
    escrow->init_slack = 0U;
    escrow->lane = 0U;
    escrow->file_size = 0U;
    escrow->attempt = 0U;
}

static bool
    flippass_module_plugin_last_skip_preserve_floor(size_t load_floor, size_t* out_preserve_floor) {
    furi_assert(out_preserve_floor);
    *out_preserve_floor = 0U;

    if(load_floor <= (FLIPPASS_PLUGIN_LAST_ALLOC_GUARD_BYTES + 1U)) {
        return false;
    }

    *out_preserve_floor = load_floor - FLIPPASS_PLUGIN_LAST_ALLOC_GUARD_BYTES - 1U;
    return true;
}

static bool flippass_module_plugin_last_shape_current_below_load(
    FlipPassPluginLastEscrow* escrow,
    size_t load_floor) {
    size_t preserve_floor = 0U;
    void* ptr = NULL;
    size_t request = 0U;
    size_t max_before = 0U;
    size_t max_after = 0U;

    furi_assert(escrow);

    if(escrow->count >= FLIPPASS_PLUGIN_LAST_MAX_ESCROWS ||
       !flippass_module_plugin_last_skip_preserve_floor(load_floor, &preserve_floor) ||
       memmgr_heap_get_max_free_block() < load_floor ||
       !flippass_module_plugin_last_reserve(
           preserve_floor, &ptr, &request, &max_before, &max_after)) {
        return false;
    }

    flippass_module_plugin_last_store_escrow(escrow, ptr, request, max_before, max_after);
    return true;
}

static bool flippass_module_plugin_last_acquire(
    App* app,
    FlipPassModuleSlot slot,
    uint8_t attempt,
    const FlipPassPluginLastProfile* profile,
    FlipPassPluginLastEscrow* escrow) {
    size_t load_floor = 0U;
    size_t placement_floor = 0U;
    size_t init_slack = 0U;
    size_t lane = 0U;
    size_t final_max = 0U;
    const char* abort_reason = "shape_incomplete";

    furi_assert(escrow);
    memset(escrow, 0, sizeof(*escrow));

    if(slot >= FlipPassModuleSlotCount || profile == NULL ||
       !flippass_module_plugin_last_compute_floors(
           profile, attempt, &load_floor, &placement_floor, &init_slack, &lane)) {
        return false;
    }

#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(app);
#endif

    escrow->load_floor = load_floor;
    escrow->placement_floor = placement_floor;
    escrow->init_slack = init_slack;
    escrow->lane = lane;
    escrow->file_size = profile->file_size;
    escrow->attempt = attempt;

    const size_t initial_max = memmgr_heap_get_max_free_block();
    if(initial_max < load_floor) {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "PLUGIN_LAST_ESCROW_SKIP slot=%s reason=below_load attempt=%u floor=%lu placement_floor=%lu init_slack=%lu lane=%lu fal=%lu max=%lu",
            flippass_module_slot_name(slot),
            attempt,
            (unsigned long)load_floor,
            (unsigned long)placement_floor,
            (unsigned long)init_slack,
            (unsigned long)lane,
            (unsigned long)profile->file_size,
            (unsigned long)initial_max);
        return false;
    }

    if(initial_max < placement_floor) {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "PLUGIN_LAST_ESCROW_SKIP slot=%s reason=below_init_slack attempt=%u floor=%lu placement_floor=%lu init_slack=%lu lane=%lu fal=%lu max=%lu",
            flippass_module_slot_name(slot),
            attempt,
            (unsigned long)load_floor,
            (unsigned long)placement_floor,
            (unsigned long)init_slack,
            (unsigned long)lane,
            (unsigned long)profile->file_size,
            (unsigned long)initial_max);
        return false;
    }

    while(escrow->count < FLIPPASS_PLUGIN_LAST_MAX_ESCROWS &&
          memmgr_heap_get_max_free_block() >= placement_floor) {
        void* ptr = NULL;
        size_t request = 0U;
        size_t max_before = 0U;
        size_t max_after = 0U;

        if(!flippass_module_plugin_last_reserve(0U, &ptr, &request, &max_before, &max_after)) {
            break;
        }

        flippass_module_plugin_last_store_escrow(escrow, ptr, request, max_before, max_after);
    }

    if(memmgr_heap_get_max_free_block() >= placement_floor) {
        abort_reason = "escrow_limit";
        goto abort;
    }

    if(escrow->count < 2U) {
        abort_reason = "no_posterior";
        goto abort;
    }

    uintptr_t skip_addr = UINTPTR_MAX;
    void* skip_ptr = NULL;
    for(uint8_t index = 0U; index < escrow->count; index++) {
        if(escrow->address[index] < skip_addr) {
            skip_addr = escrow->address[index];
            skip_ptr = escrow->ptr[index];
        }
    }

    uintptr_t target_addr = UINTPTR_MAX;
    void* target_ptr = NULL;
    for(uint8_t index = 0U; index < escrow->count; index++) {
        if(escrow->ptr[index] != skip_ptr && escrow->address[index] > skip_addr &&
           escrow->address[index] < target_addr) {
            target_addr = escrow->address[index];
            target_ptr = escrow->ptr[index];
        }
    }

    if(target_ptr == NULL) {
        abort_reason = "no_later_target";
        goto abort;
    }

    if(!flippass_module_plugin_last_forget_ptr(escrow, skip_ptr)) {
        abort_reason = "skip_missing";
        goto abort;
    }
    free(skip_ptr);
    skip_ptr = NULL;

    if(!flippass_module_plugin_last_shape_current_below_load(escrow, load_floor)) {
        abort_reason = "skip_shape_fail";
        goto abort;
    }

    while(memmgr_heap_get_max_free_block() >= load_floor) {
        if(!flippass_module_plugin_last_shape_current_below_load(escrow, load_floor)) {
            abort_reason = "skip_incomplete";
            goto abort;
        }
    }

    if(!flippass_module_plugin_last_forget_ptr(escrow, target_ptr)) {
        abort_reason = "target_missing";
        goto abort;
    }
    free(target_ptr);
    target_ptr = NULL;

    final_max = memmgr_heap_get_max_free_block();
    if(final_max >= placement_floor) {
        return true;
    }

    abort_reason = "target_too_small";

abort:
#if FLIPPASS_EVENT_LOG_ENABLED
    const uint8_t abort_count = escrow->count;
#endif
    flippass_module_plugin_last_release(app, slot, escrow, abort_reason);
#if FLIPPASS_EVENT_LOG_ENABLED
    FLIPPASS_PROBE_LOG_EVENT(
        app,
        "PLUGIN_LAST_ESCROW_ABORT slot=%s reason=%s count=%u floor=%lu placement_floor=%lu init_slack=%lu lane=%lu fal=%lu final_max=%lu",
        flippass_module_slot_name(slot),
        abort_reason,
        abort_count,
        (unsigned long)load_floor,
        (unsigned long)placement_floor,
        (unsigned long)init_slack,
        (unsigned long)lane,
        (unsigned long)profile->file_size,
        (unsigned long)final_max);
#endif
    return false;
}

void flippass_module_loader_init(App* app) {
    furi_assert(app);

    memset(&app->module_loader, 0, sizeof(app->module_loader));
    app->module_loader.storage = furi_record_open(RECORD_STORAGE);
    /* The first interface's version is the one checked against each plugin. */
    app->module_loader.api_resolver = composite_api_resolver_alloc();
    composite_api_resolver_add(app->module_loader.api_resolver, firmware_api_interface);
    composite_api_resolver_add(app->module_loader.api_resolver, flippass_app_api_interface);
}

void flippass_module_pin(App* app, FlipPassModuleSlot slot) {
    furi_assert(app);
    furi_assert(slot < FlipPassModuleSlotCount);
    furi_check(app->module_loader.slot[slot].pin_count < UINT8_MAX);
    app->module_loader.slot[slot].pin_count++;
}

void flippass_module_unpin(App* app, FlipPassModuleSlot slot) {
    furi_assert(app);
    furi_assert(slot < FlipPassModuleSlotCount);
    furi_check(app->module_loader.slot[slot].pin_count > 0U);
    app->module_loader.slot[slot].pin_count--;
}

void flippass_module_set_sticky(App* app, FlipPassModuleSlot slot, bool sticky) {
    furi_assert(app);
    furi_assert(slot < FlipPassModuleSlotCount);
    app->module_loader.slot[slot].sticky = sticky &&
                                           app->module_loader.slot[slot].application != NULL;
}

void flippass_module_unload(App* app, FlipPassModuleSlot slot) {
    furi_assert(app);
    furi_assert(slot < FlipPassModuleSlotCount);

    FlipPassModuleInstance* instance = &app->module_loader.slot[slot];
    if(instance->pin_count != 0U) {
        /* Its code is still executing: unmapping it now would fault on return. */
        return;
    }
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
    instance->sticky = false;
}

void flippass_module_unload_all_runtime(App* app) {
    furi_assert(app);

    for(size_t index = 0U; index < FlipPassModuleSlotCount; index++) {
        if(!app->module_loader.slot[index].sticky) {
            flippass_module_unload(app, (FlipPassModuleSlot)index);
        }
    }
}

void flippass_module_loader_deinit(App* app) {
    furi_assert(app);

    for(size_t index = 0U; index < FlipPassModuleSlotCount; index++) {
        app->module_loader.slot[index].pin_count = 0U;
        app->module_loader.slot[index].sticky = false;
    }
    flippass_module_unload_all_runtime(app);

    if(app->module_loader.api_resolver != NULL) {
        composite_api_resolver_free(app->module_loader.api_resolver);
        app->module_loader.api_resolver = NULL;
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
#if FLIPPASS_EVENT_LOG_ENABLED
        flippass_module_log_load_fail(app, slot, "storage", NULL, "unavailable");
#endif
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass plugin storage is unavailable.");
        }
        return NULL;
    }

    const char* module_path = (path != NULL) ? path : flippass_module_default_path(slot);
    const char* module_name = flippass_module_path_basename(module_path);
    if(module_path == NULL) {
#if FLIPPASS_EVENT_LOG_ENABLED
        flippass_module_log_load_fail(app, slot, "path", NULL, "undefined");
#endif
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass plugin path is not defined.");
        }
        return NULL;
    }

    FlipPassPluginLastProfile plugin_last_profile;
    const bool plugin_last_profile_ready =
        !FLIPPASS_MODULE_DIRECT_LOAD &&
        flippass_module_plugin_last_profile(
            app, app->module_loader.storage, slot, module_path, &plugin_last_profile);
    bool memory_retry_used = false;
    uint8_t attempt = 0U;

    while(attempt < FLIPPASS_MODULE_LOAD_ATTEMPTS) {
        FlipPassPluginLastEscrow plugin_last_escrow;
        memset(&plugin_last_escrow, 0, sizeof(plugin_last_escrow));

        if(plugin_last_profile_ready && attempt < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
            flippass_module_plugin_last_acquire(
                app, slot, attempt, &plugin_last_profile, &plugin_last_escrow);
        }

        FlipperApplication* application = flipper_application_alloc(
            app->module_loader.storage,
            composite_api_resolver_get(app->module_loader.api_resolver));
        if(application == NULL) {
            const bool placement_active = plugin_last_escrow.count > 0U;
            flippass_module_plugin_last_release(app, slot, &plugin_last_escrow, "alloc_fail");
#if FLIPPASS_EVENT_LOG_ENABLED
            flippass_module_log_load_fail(app, slot, "alloc", module_path, "null");
#endif
            if(placement_active && attempt + 1U < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
                attempt++;
                continue;
            }
            if(!memory_retry_used &&
               flippass_module_prepare_memory_retry(app, slot, "alloc", module_path, "null")) {
                memory_retry_used = true;
                attempt = 0U;
                continue;
            }
            if(placement_active && attempt < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
                attempt = FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS;
                continue;
            }
            if(error != NULL) {
                furi_string_set_str(error, "FlipPass could not allocate the requested plugin.");
            }
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
            return NULL;
        }
        if(plugin_last_escrow.count == 0U) {
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_alloc_ok", slot, 0U);
        }

        const FlipperApplicationPreloadStatus preload_status =
            flipper_application_preload(application, module_path);
        if(preload_status != FlipperApplicationPreloadStatusSuccess) {
            const char* preload_status_text =
                flipper_application_preload_status_to_string(preload_status);
            const bool placement_active = plugin_last_escrow.count > 0U;
            const bool memory_failure = preload_status ==
                                        FlipperApplicationPreloadStatusNotEnoughMemory;
            flippass_module_plugin_last_release(app, slot, &plugin_last_escrow, "preload_fail");
            flipper_application_free(application);
#if FLIPPASS_EVENT_LOG_ENABLED
            flippass_module_log_load_fail(app, slot, "preload", module_path, preload_status_text);
#endif
            if(memory_failure && placement_active &&
               attempt + 1U < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
                attempt++;
                continue;
            }
            if(memory_failure && !memory_retry_used &&
               flippass_module_prepare_memory_retry(
                   app, slot, "preload", module_path, preload_status_text)) {
                memory_retry_used = true;
                attempt = 0U;
                continue;
            }
            if(memory_failure && placement_active &&
               attempt < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
                attempt = FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS;
                continue;
            }
            if(error != NULL) {
                furi_string_printf(
                    error, "FlipPass could not preload %s (%s).", module_name, preload_status_text);
            }
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
            return NULL;
        }
        if(plugin_last_escrow.count == 0U) {
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_preload_ok", slot, 0U);
        }

        if(!flipper_application_is_plugin(application)) {
            flippass_module_plugin_last_release(
                app, slot, &plugin_last_escrow, "plugin_check_fail");
#if FLIPPASS_EVENT_LOG_ENABLED
            flippass_module_log_load_fail(app, slot, "plugin_check", module_path, "not_plugin");
#endif
            if(error != NULL) {
                furi_string_printf(error, "%s is not a FlipPass plugin.", module_name);
            }
            flipper_application_free(application);
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
            return NULL;
        }

        if(plugin_last_escrow.count == 0U) {
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_map_begin", slot, 0U);
        }
        const FlipperApplicationLoadStatus load_status =
            flipper_application_map_to_memory(application);
        if(load_status != FlipperApplicationLoadStatusSuccess) {
            const char* load_status_text = flipper_application_load_status_to_string(load_status);
            const bool placement_active = plugin_last_escrow.count > 0U;
            const bool memory_failure = load_status != FlipperApplicationLoadStatusMissingImports;
            flippass_module_plugin_last_release(app, slot, &plugin_last_escrow, "map_fail");
            flipper_application_free(application);
#if FLIPPASS_EVENT_LOG_ENABLED
            flippass_module_log_load_fail(app, slot, "map", module_path, load_status_text);
#endif
            if(memory_failure && placement_active &&
               attempt + 1U < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
                attempt++;
                continue;
            }
            if(memory_failure && !memory_retry_used &&
               flippass_module_prepare_memory_retry(
                   app, slot, "map", module_path, load_status_text)) {
                memory_retry_used = true;
                attempt = 0U;
                continue;
            }
            if(memory_failure && placement_active &&
               attempt < FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS) {
                attempt = FLIPPASS_PLUGIN_LAST_PLACEMENT_ATTEMPTS;
                continue;
            }
            if(error != NULL) {
                furi_string_printf(
                    error, "FlipPass could not map %s (%s).", module_name, load_status_text);
            }
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
            return NULL;
        }

        flippass_module_plugin_last_release(app, slot, &plugin_last_escrow, "map_ok");
        FLIPPASS_MEMORY_LOG_MODULE(app, "module_map_ok", slot, 0U);

        const FlipperAppPluginDescriptor* descriptor =
            flipper_application_plugin_get_descriptor(application);
        if(descriptor == NULL) {
#if FLIPPASS_EVENT_LOG_ENABLED
            flippass_module_log_load_fail(app, slot, "descriptor", module_path, "null");
#endif
            if(error != NULL) {
                furi_string_printf(error, "%s did not expose a plugin descriptor.", module_name);
            }
            flipper_application_free(application);
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
            return NULL;
        }

        if(expected_appid != NULL && strcmp(descriptor->appid, expected_appid) != 0) {
#if FLIPPASS_EVENT_LOG_ENABLED
            flippass_module_log_load_fail(app, slot, "appid", module_path, descriptor->appid);
#endif
            if(error != NULL) {
                furi_string_printf(
                    error,
                    "FlipPass loaded %s but received plugin appid %s.",
                    module_name,
                    descriptor->appid);
            }
            flipper_application_free(application);
            FLIPPASS_MEMORY_LOG_MODULE(app, "module_load_fail", slot, 0U);
            return NULL;
        }

        if(expected_api_version != 0U && descriptor->ep_api_version != expected_api_version) {
#if FLIPPASS_EVENT_LOG_ENABLED
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
#if FLIPPASS_EVENT_LOG_ENABLED
        const size_t free_heap = memmgr_get_free_heap();
        const size_t max_free = memmgr_heap_get_max_free_block();
        FLIPPASS_PROBE_LOG_EVENT(
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

    return NULL;
}
