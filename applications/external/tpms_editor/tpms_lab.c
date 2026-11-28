#include "tpms_app_i.h"
#include "tpms_save.h"
#include "tpms_editor_tx.h"
#include "tpms_encoder.h"
#include "protocols/tpms_generic.h"
#include "protocols/proto6/custom_presets.h"
#include <flipper_format/flipper_format_i.h>
#include <lib/toolbox/stream/stream.h>
#include <storage/storage.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define TPMS_LAB_PROFILE_COUNT 6U
#define TPMS_LAB_LEGACY_SAVED_MAX 10U

static void tpms_lab_slot_path(uint8_t slot, char* path, size_t size) {
    snprintf(path, size, EXT_PATH("apps_data/tpms_editor/.lab_session_%02u.tpl"), slot);
}

void tpms_lab_clear_session_files(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    char path[96];
    for(uint8_t i = 0U; i < TPMS_LAB_MAX_SENSORS; i++) {
        tpms_lab_slot_path(i, path, sizeof(path));
        if(storage_file_exists(storage, path)) (void)storage_common_remove(storage, path);
    }
    furi_record_close(RECORD_STORAGE);
}

static uint32_t tpms_lab_hash(const char* protocol, uint32_t id) {
    uint32_t h = 2166136261UL;
    const char* p = protocol ? protocol : "";
    while(*p) {
        h ^= (uint8_t)*p++;
        h *= 16777619UL;
    }
    h ^= id;
    h *= 16777619UL;
    return h ? h : 1U;
}

static bool tpms_lab_has_tpl_extension(const char* name) {
    const size_t len = name ? strlen(name) : 0U;
    if(len < 4U) return false;
    const char* ext = name + len - 4U;
    return ext[0] == '.' &&
           (ext[1] == 't' || ext[1] == 'T') &&
           (ext[2] == 'p' || ext[2] == 'P') &&
           (ext[3] == 'l' || ext[3] == 'L');
}

static void tpms_lab_legacy_saved_slot_path(uint8_t slot, char* path, size_t size) {
    snprintf(path, size, EXT_PATH("apps_data/tpms_editor/lab_saved_%02u.tpl"), slot);
}

static void tpms_lab_saved_profile_path(
    const TpmsEditValues* values, const char* protocol, char* path, size_t size) {
    const uint32_t key = tpms_lab_hash(protocol, values ? values->id : 0U);
    snprintf(
        path,
        size,
        "%s/%08lX_%08lX.tpl",
        TPMS_LAB_SAVED_FOLDER,
        (unsigned long)key,
        (unsigned long)(values ? values->id : 0U));
}

static bool tpms_lab_saved_store_internal(
    const TpmsEditValues* values, const char* protocol, const char* label) {
    if(!values || !protocol || !protocol[0]) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_LAB_SAVED_FOLDER);
    furi_record_close(RECORD_STORAGE);

    char path[160];
    tpms_lab_saved_profile_path(values, protocol, path, sizeof(path));
    return tpms_profile_save_to_path_named(path, values, protocol, label);
}

void tpms_lab_saved_migrate_legacy(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_LAB_SAVED_FOLDER);
    furi_record_close(RECORD_STORAGE);

    for(uint8_t slot = 0U; slot < TPMS_LAB_LEGACY_SAVED_MAX; slot++) {
        char legacy_path[96];
        tpms_lab_legacy_saved_slot_path(slot, legacy_path, sizeof(legacy_path));

        Storage* check_storage = furi_record_open(RECORD_STORAGE);
        if(!check_storage) return;
        const bool exists = storage_file_exists(check_storage, legacy_path);
        furi_record_close(RECORD_STORAGE);
        if(!exists) continue;

        TpmsEditValues values = {0};
        char protocol[48] = {0};
        if(tpms_profile_load(legacy_path, &values, protocol, sizeof(protocol)) &&
           tpms_lab_saved_store_internal(&values, protocol, NULL)) {
            Storage* remove_storage = furi_record_open(RECORD_STORAGE);
            if(remove_storage) {
                (void)storage_common_remove(remove_storage, legacy_path);
                furi_record_close(RECORD_STORAGE);
            }
        }
    }
}

uint16_t tpms_lab_saved_count(void) {
    tpms_lab_saved_migrate_legacy();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return 0U;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_LAB_SAVED_FOLDER);

    File* dir = storage_file_alloc(storage);
    uint16_t count = 0U;
    if(dir && storage_dir_open(dir, TPMS_LAB_SAVED_FOLDER)) {
        FileInfo info;
        char name[96];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info) || !tpms_lab_has_tpl_extension(name)) continue;
            if(count < UINT16_MAX) count++;
        }
        storage_dir_close(dir);
    }
    if(dir) storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return count;
}

bool tpms_lab_saved_store(const TpmsEditValues* values, const char* protocol) {
    tpms_lab_saved_migrate_legacy();
    return tpms_lab_saved_store_internal(values, protocol, NULL);
}

bool tpms_lab_saved_store_named(
    const TpmsEditValues* values, const char* protocol, const char* label) {
    tpms_lab_saved_migrate_legacy();
    return tpms_lab_saved_store_internal(values, protocol, label);
}

bool tpms_lab_saved_delete(const char* path) {
    if(!path || !path[0]) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    const bool exists = storage_file_exists(storage, path);
    bool ok = false;
    if(exists) {
        (void)storage_common_remove(storage, path);
        ok = !storage_file_exists(storage, path);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tpms_lab_session_load(
    uint8_t slot, TpmsEditValues* values, char* protocol, size_t protocol_size) {
    if(slot >= TPMS_LAB_MAX_SENSORS || !values || !protocol || protocol_size == 0U) return false;
    char path[96];
    tpms_lab_slot_path(slot, path, sizeof(path));
    return tpms_profile_load(path, values, protocol, protocol_size);
}

bool tpms_lab_session_delete(uint8_t slot) {
    if(slot >= TPMS_LAB_MAX_SENSORS) return false;
    char path[96];
    tpms_lab_slot_path(slot, path, sizeof(path));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    const bool exists = storage_file_exists(storage, path);
    bool ok = false;
    if(exists) {
        (void)storage_common_remove(storage, path);
        ok = !storage_file_exists(storage, path);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

uint8_t tpms_lab_session_count(void) {
    uint8_t count = 0U;
    TpmsEditValues values;
    char protocol[48];
    for(uint8_t slot = 0U; slot < TPMS_LAB_MAX_SENSORS; slot++) {
        memset(&values, 0, sizeof(values));
        memset(protocol, 0, sizeof(protocol));
        if(tpms_lab_session_load(slot, &values, protocol, sizeof(protocol))) count++;
    }
    return count;
}

static int8_t tpms_lab_find_key(const TPMSApp* app, uint32_t key) {
    for(uint8_t i = 0U; i < app->lab_sensor_count; i++) {
        if(app->lab_sensor_keys[i] == key) return (int8_t)i;
    }
    return -1;
}

static uint8_t tpms_lab_current_radio_preset(const TPMSApp* app) {
    if(!app || !app->txrx || !app->txrx->preset || !app->txrx->preset->name)
        return TpmsRadioPresetAuto;
    const char* name = furi_string_get_cstr(app->txrx->preset->name);
    if(strstr(name, "GFSK")) return TpmsRadioPresetGFSK;
    if(strstr(name, "OOK")) return TpmsRadioPresetOOK;
    if(strstr(name, "FSK")) return TpmsRadioPresetFSK;
    return TpmsRadioPresetAuto;
}

/* profile: 0..2 = 315 MHz FSK/OOK/GFSK, 3..5 = 433.92 MHz FSK/OOK/GFSK */
static void tpms_lab_apply_profile(TPMSApp* app, uint8_t profile) {
    profile %= TPMS_LAB_PROFILE_COUNT;
    const bool band433 = profile >= 3U;
    const uint8_t modulation = profile % 3U;
    const uint32_t frequency = band433 ? 433920000U : 315000000U;

    const char* preset_name = "TPMS FSK";
    uint8_t* preset_data = (uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
    size_t preset_size = protoview_subghz_tpms1_fsk_async_regs_size;
    Proto6Mode bridge_mode = Proto6ModeFSK;

    if(modulation == 1U) {
        preset_name = "TPMS OOK";
        preset_data = (uint8_t*)protoview_subghz_tpms2_ook_async_regs;
        preset_size = protoview_subghz_tpms2_ook_async_regs_size;
        bridge_mode = Proto6ModeOOK;
    } else if(modulation == 2U) {
        preset_name = "TPMS GFSK";
        preset_data = (uint8_t*)protoview_subghz_tpms3_gfsk_async_regs;
        preset_size = protoview_subghz_tpms3_gfsk_async_regs_size;
    }

    tpms_preset_init(app, preset_name, frequency, preset_data, preset_size);
    proto6_bridge_set_mode(app->txrx->proto6, bridge_mode);
}

static bool tpms_lab_profile_matches(const TPMSApp* app, uint8_t profile) {
    const bool band433 = profile >= 3U;
    const uint8_t mod = profile % 3U; /* 0 FSK, 1 OOK, 2 GFSK */

    bool band_ok = app->lab_band_index == 0U;
    if(app->lab_band_index == 1U) band_ok = !band433;
    else if(app->lab_band_index == 2U) band_ok = band433;

    bool mod_ok = app->lab_modulation_index == 0U;
    if(app->lab_modulation_index > 0U) mod_ok = (app->lab_modulation_index - 1U) == mod;
    return band_ok && mod_ok;
}

static uint8_t tpms_lab_choose_profile(TPMSApp* app) {
    /* EURO/default first: 433 FSK, then paired 315 FSK, 433 OOK, 315 OOK,
       433 GFSK, 315 GFSK. The radio is reconfigured only between 5 s RX
       windows, never while async RX is active. */
    static const uint8_t order[TPMS_LAB_PROFILE_COUNT] = {3U, 0U, 4U, 1U, 5U, 2U};
    const uint8_t start = app->lab_auto_profile_index % TPMS_LAB_PROFILE_COUNT;
    for(uint8_t off = 0U; off < TPMS_LAB_PROFILE_COUNT; off++) {
        const uint8_t oi = (uint8_t)((start + off) % TPMS_LAB_PROFILE_COUNT);
        const uint8_t profile = order[oi];
        if(tpms_lab_profile_matches(app, profile)) {
            app->lab_auto_profile_index = (uint8_t)((oi + 1U) % TPMS_LAB_PROFILE_COUNT);
            return profile;
        }
    }
    return 3U;
}

bool tpms_lab_prepare(TPMSApp* app) {
    if(!app) return false;
    tpms_lab_release(app, false);
    memset(app->lab_sensor_keys, 0, sizeof(app->lab_sensor_keys));
    app->lab_sensor_count = 0U;
    app->lab_round = 0U;
    app->lab_last_tx_ok = 0U;
    app->lab_last_tx_frequency = 0U;
    app->lab_last_tx_preset = TpmsRadioPresetAuto;
    app->lab_last_tx_kind = TpmsEncoderNone;
    app->lab_auto_profile_index = 0U;
    tpms_lab_clear_session_files();
    app->lab_format = flipper_format_string_alloc();
    app->lab_protocol = furi_string_alloc();
    if(!app->lab_format || !app->lab_protocol) {
        tpms_lab_release(app, true);
        return false;
    }
    return true;
}

void tpms_lab_stop_rx(TPMSApp* app) {
    if(!app || !app->txrx) return;
    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    if(app->txrx->proto6) proto6_bridge_reset(app->txrx->proto6);
}

void tpms_lab_release(TPMSApp* app, bool clear_files) {
    if(!app) return;
    tpms_lab_stop_rx(app);
    if(app->lab_protocol) {
        furi_string_free(app->lab_protocol);
        app->lab_protocol = NULL;
    }
    if(app->lab_format) {
        flipper_format_free(app->lab_format);
        app->lab_format = NULL;
    }
    if(clear_files) tpms_lab_clear_session_files();
}

bool tpms_lab_start_rx(TPMSApp* app) {
    if(!app || !app->txrx || !app->txrx->proto6 || !app->txrx->preset) return false;
    tpms_lab_stop_rx(app);
    if(!tpms_radio_ensure_selected(app)) return false;

    const uint8_t profile = tpms_lab_choose_profile(app);
    tpms_lab_apply_profile(app, profile);
    proto6_bridge_reset(app->txrx->proto6);
    tpms_begin(app, app->txrx->preset->data);
    tpms_rx(app, app->txrx->preset->frequency);
    return app->txrx->txrx_state == TPMSTxRxStateRx;
}

bool tpms_lab_capture_decoder(TPMSApp* app, SubGhzProtocolDecoderBase* decoder) {
    if(!app || !decoder || !app->lab_format || !app->lab_protocol || !app->txrx ||
       !app->txrx->preset) return false;
    Stream* stream = flipper_format_get_raw_stream(app->lab_format);
    if(stream) stream_clean(stream);
    if(subghz_protocol_decoder_base_serialize(decoder, app->lab_format, app->txrx->preset) !=
       SubGhzProtocolStatusOk) return false;
    furi_string_reset(app->lab_protocol);
    flipper_format_rewind(app->lab_format);
    if(!flipper_format_read_string(app->lab_format, "Protocol", app->lab_protocol)) return false;

    TPMSBlockGeneric generic = {0};
    if(tpms_block_generic_deserialize(&generic, app->lab_format) != SubGhzProtocolStatusOk) return false;
    const char* protocol = furi_string_get_cstr(app->lab_protocol);
    TpmsEditValues values = {0};
    values.kind = tpms_encoder_for_model(protocol);
    if(values.kind == TpmsEncoderNone) return false;
    values.id = generic.id;
    values.pressure_kpa = generic.pressure * 100.0f;
    values.temperature_c = generic.temperature;
    values.frequency_hz = app->txrx->preset->frequency;
    values.radio_preset = tpms_lab_current_radio_preset(app);
    values.repeat_count = 3U;
    if(values.kind == TpmsEncoderSchraderEG53MA4) {
        values.flags = (uint32_t)(generic.data >> 32U);
        values.temperature_raw_f = (uint8_t)((generic.data >> 24U) & 0xFFU);
        values.temperature_raw_f_valid = true;
        values.temperature_c = ((float)values.temperature_raw_f - 32.0f) * (5.0f / 9.0f);
    } else {
        values.flags = (uint32_t)((generic.data >> 48U) & 0xFFFFU);
        values.temperature_raw_f_valid = false;
    }
    values.edit_mask = 0U;
    values.raw_payload_size = generic.raw_payload_size;
    values.raw_payload_bits = generic.raw_payload_bits;
    values.raw_payload_valid = generic.raw_payload_valid;
    if(generic.raw_payload_valid && generic.raw_payload_size > 0U &&
       generic.raw_payload_size <= sizeof(values.raw_payload)) {
        memcpy(values.raw_payload, generic.raw_payload, generic.raw_payload_size);
    }

    const uint32_t key = tpms_lab_hash(protocol, values.id);
    int8_t existing = tpms_lab_find_key(app, key);
    uint8_t slot;
    if(existing >= 0) {
        slot = (uint8_t)existing;
    } else {
        if(app->lab_sensor_count >= TPMS_LAB_MAX_SENSORS) return false;
        slot = app->lab_sensor_count;
        app->lab_sensor_keys[slot] = key;
        app->lab_sensor_count++;
    }
    char path[96];
    tpms_lab_slot_path(slot, path, sizeof(path));
    /* AUTO LAB capture is session-only. It does not consume a persistent
       saved slot. The user can explicitly save any captured sensor later. */
    return tpms_profile_save_to_path(path, &values, protocol);
}

uint8_t tpms_lab_send_burst(TPMSApp* app) {
    if(!app) return 0U;
    tpms_lab_stop_rx(app);
    notification_message(app->notifications, &sequence_set_only_blue_255);

    uint8_t ok_count = 0U;
    char path[96];
    char protocol[48];
    for(uint8_t i = 0U; i < app->lab_sensor_count; i++) {
        TpmsEditValues values = {0};
        tpms_lab_slot_path(i, path, sizeof(path));
        if(!tpms_profile_load(path, &values, protocol, sizeof(protocol))) continue;
        values.pressure_kpa = app->lab_target_pressure_kpa;
        values.edit_mask |= TPMS_EDIT_PRESSURE;
        if(values.repeat_count == 0U) values.repeat_count = 3U;
        app->lab_last_tx_frequency = values.frequency_hz;
        app->lab_last_tx_preset = values.radio_preset;
        app->lab_last_tx_kind = (uint8_t)values.kind;
        if(tpms_editor_send(app, &values)) ok_count++;
    }
    notification_message(app->notifications, &sequence_reset_rgb);
    app->lab_last_tx_ok = ok_count;
    app->lab_round++;
    return ok_count;
}
