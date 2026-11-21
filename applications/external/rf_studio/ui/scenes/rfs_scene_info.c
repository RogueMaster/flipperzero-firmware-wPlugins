#include "../../rf_studio_i.h"

/* Diagnostics screen (Specification Section 13.9). Results read from the firmware are reported as
 * found; nothing here is inferred or hardcoded. */
static void rfs_info_diagnostics(RfStudioApp* app, FuriString* out) {
    const RfsCapabilities* caps = rfs_radio_caps(app->radio);
    const RfsFirmwareInfo* fw = &caps->firmware;

    furi_string_cat_printf(
        out,
        "FIRMWARE\n %s %s\n Commit %s\n Built %s, f%u\n Adapter: %s\n TX reasons: %s\n\n",
        fw->origin,
        fw->version,
        fw->githash,
        fw->build_date,
        fw->target,
        rfs_firmware_family_name(fw->family),
        fw->has_check_tx ? "yes" : "no");

    furi_string_cat_str(out, "RADIOS (CC1101)\n");
    for(RfsDeviceId id = 0; id < RfsDeviceCount; id++) {
        const char* status = !caps->device_present[id]  ? "no driver" :
                             caps->device_connected[id] ? "ready" :
                                                          "not found";
        furi_string_cat_printf(out, " %s: %s\n", rfs_radio_device_label(id), status);
    }
    furi_string_cat_printf(out, " Mode: %s\n\n", rfs_radio_mode_name(app->settings.radio_mode));

    size_t count = rfs_radio_freq_count(app->radio, RfsDiscoverAll);
    uint32_t low = UINT32_MAX, high = 0;
    size_t tx_allowed = 0;
    const SubGhzDevice* internal = rfs_radio_device(app->radio, RfsDeviceInternal);
    for(size_t i = 0; i < count; i++) {
        uint32_t f = rfs_radio_freq_get(app->radio, RfsDiscoverAll, i);
        if(f < low) low = f;
        if(f > high) high = f;
        if(internal && rfs_firmware_check_tx(internal, f) == RfsTxPermAllowed) tx_allowed++;
    }
    char low_mhz[16], high_mhz[16];
    rfs_format_mhz(count ? low : 0, low_mhz, sizeof(low_mhz));
    rfs_format_mhz(high, high_mhz, sizeof(high_mhz));
    furi_string_cat_printf(
        out,
        "FREQUENCIES (firmware)\n %u listed, %u common\n %s-%s MHz\n TX allowed: %u of %u\n\n",
        (unsigned)count,
        (unsigned)rfs_radio_freq_count(app->radio, RfsDiscoverCommon),
        low_mhz,
        high_mhz,
        (unsigned)tx_allowed,
        (unsigned)count);

    size_t presets = rfs_radio_preset_count(app->radio);
    furi_string_cat_printf(out, "MODULATION PRESETS (%u)\n", (unsigned)presets);
    for(size_t i = 0; i < presets; i++) {
        furi_string_cat_printf(out, " %s\n", rfs_radio_preset_name(app->radio, i));
    }

    RfsProtocolStats stats;
    rfs_protocol_stats(&stats);
    furi_string_cat_printf(
        out,
        "\nPROTOCOLS\n %u total: %u fixed,\n %u rolling, %u raw\n %u can transmit\n Raw capture: %s\n Raw replay: %s\n\n",
        (unsigned)stats.total,
        (unsigned)stats.fixed,
        (unsigned)stats.rolling,
        (unsigned)stats.raw,
        (unsigned)stats.sendable,
        caps->raw_capture ? "yes" : "no",
        caps->raw_transmit ? "yes" : "no");

    uint64_t total = 0, free_bytes = 0;
    furi_string_cat_str(out, "STORAGE\n");
    if(storage_common_fs_info(app->storage, STORAGE_EXT_PATH_PREFIX, &total, &free_bytes) ==
       FSE_OK) {
        furi_string_cat_printf(
            out,
            " SD: %lu/%lu MB free\n",
            (unsigned long)(free_bytes / (1024 * 1024)),
            (unsigned long)(total / (1024 * 1024)));
    } else {
        furi_string_cat_str(out, " SD card not available\n");
    }
    RfsRemoteList list;
    rfs_remote_list_init(&list);
    rfs_remote_list_load(app->storage, &list);
    furi_string_cat_printf(
        out,
        " Remotes: %u  Signals: %u\n Free memory: %u KB\n\n",
        (unsigned)list.count,
        (unsigned)app->signals.count,
        (unsigned)(memmgr_get_free_heap() / 1024));
    rfs_remote_list_clear(&list);

    furi_string_cat_str(out, "RECENT ERRORS\n");
    rfs_diag_errors_to_string(app->diag, out);
}

void rfs_scene_info_on_enter(void* context) {
    RfStudioApp* app = context;
    furi_string_reset(app->text_store);

    if(app->info_kind == RfsInfoDiagnostics) {
        rfs_info_diagnostics(app, app->text_store);
    } else {
        RfsSignalMeta* meta = rfs_signal_find(&app->signals, app->signal_id);
        if(meta) {
            rfs_signal_describe(meta, app->text_store);
            FuriString* path = furi_string_alloc();
            rfs_signal_path(meta->id, path);
            if(!storage_file_exists(app->storage, furi_string_get_cstr(path))) {
                furi_string_cat_str(app->text_store, "\nWARNING: file is missing\n");
            }
            furi_string_free(path);
            furi_string_cat_str(app->text_store, "\nUsed by:\n");
            if(rfs_remote_signal_usage(app->storage, meta->id, app->text_store) == 0) {
                furi_string_cat_str(app->text_store, " (no buttons)\n");
            }
        } else {
            furi_string_set(app->text_store, "Signal not found.");
        }
    }

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewTextBox);
}

bool rfs_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rfs_scene_info_on_exit(void* context) {
    RfStudioApp* app = context;
    text_box_reset(app->text_box);
}
