#include "tpms_save.h"
#include "tpms_app_i.h"
#include "protocols/tpms_generic.h"
#include "protocols/proto6/custom_presets.h"

#include <storage/storage.h>
#include <lib/flipper_format/flipper_format_i.h>
#include <lib/subghz/receiver.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define TAG "TPMSSave"
#define TPMS_SAVE_FOLDER EXT_PATH("subghz/TPMS_TEST")
#define TPMS_SAVE_FOLDER_ORG EXT_PATH("subghz/TPMS_TEST/ORG")
#define TPMS_SAVE_FOLDER_MOD EXT_PATH("subghz/TPMS_TEST/MOD")
#define TPMS_RAW_FILE_TYPE "Flipper SubGhz RAW File"
#define TPMS_RAW_FILE_VERSION 1U
#define TPMS_RAW_CHUNK 64U
#define TPMS_REPEAT_GAP_US 10000U

typedef struct {
    FlipperFormat* format;
    int32_t chunk[TPMS_RAW_CHUNK];
    size_t count;
    int32_t pending;
    bool has_pending;
    bool ok;
} TpmsRawWriter;

/* TPMS 4.3: optional, human-readable Ford RAW diagnostics. The canonical
   source remains RawPayload; these keys merely make field comparison easier
   outside the app and deliberately do not assign a meaning to 0x80/0x10. */
static bool tpms_profile_write_ford_diagnostics(
    FlipperFormat* format, const TpmsEditValues* values) {
    if(!format || !values || values->kind != TpmsEncoderFord ||
       !values->raw_payload_valid || values->raw_payload_size < 7U) {
        return true;
    }

    const uint8_t tt = values->raw_payload[5];
    const uint8_t ff = values->raw_payload[6];
    const bool temp_valid = ((tt & 0x80U) == 0U) && ((ff & 0x0FU) != 0x0BU);
    const int temp_calc_c = (int)tt - 56;
    const char* tt_mode = (tt & 0x80U) ? "SPECIAL_MSB" :
                          ((ff & 0x0FU) == 0x0BU) ? "FF_xB_NO_TEMP" :
                          (temp_calc_c < -40 ? "TEMP_LOW?" : "TEMP");

    uint32_t u32 = tt;
    if(!flipper_format_write_uint32(format, "FordTTRaw", &u32, 1U)) return false;
    u32 = ff;
    if(!flipper_format_write_uint32(format, "FordFFRaw", &u32, 1U)) return false;
    if(!flipper_format_write_string_cstr(format, "FordTTMode", tt_mode)) return false;
    u32 = temp_valid ? 1U : 0U;
    if(!flipper_format_write_uint32(format, "FordTTValid", &u32, 1U)) return false;

    float temp_f = (float)temp_calc_c;
    if(!flipper_format_write_float(format, "FordTTCalcC", &temp_f, 1U)) return false;

    static const struct {
        const char* key;
        uint8_t mask;
    } bits[] = {
        {"FordBit80", 0x80U},
        {"FordBit40Moving", 0x40U},
        {"FordBit20Pressure9", 0x20U},
        {"FordBit10", 0x10U},
        {"FordBit08Learn", 0x08U},
        {"FordBit04", 0x04U},
        {"FordBit02", 0x02U},
        {"FordBit01", 0x01U},
    };
    for(size_t i = 0U; i < COUNT_OF(bits); i++) {
        u32 = (ff & bits[i].mask) ? 1U : 0U;
        if(!flipper_format_write_uint32(format, bits[i].key, &u32, 1U)) return false;
    }
    return true;
}

static bool tpms_save_make_datetime_path(
    Storage* storage,
    const char* folder,
    const char* extension,
    FuriString* path) {
    if(!storage || !folder || !extension || !path) return false;
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    furi_string_printf(
        path,
        "%s/%04u-%02u-%02u_%02u-%02u-%02u.%s",
        folder,
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second,
        extension);

    for(uint16_t n = 2U; storage_file_exists(storage, furi_string_get_cstr(path)); n++) {
        furi_string_printf(
            path,
            "%s/%04u-%02u-%02u_%02u-%02u-%02u_%u.%s",
            folder,
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day,
            (unsigned)dt.hour,
            (unsigned)dt.minute,
            (unsigned)dt.second,
            n,
            extension);
        if(n == UINT16_MAX) return false;
    }
    return true;
}

static void tpms_save_sanitize_label(const char* label, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!label || !label[0]) return;

    size_t j = 0U;
    bool previous_separator = false;
    for(size_t i = 0U; label[i] && j + 1U < out_size; i++) {
        const unsigned char c = (unsigned char)label[i];
        if(isalnum(c)) {
            out[j++] = (char)c;
            previous_separator = false;
        } else if(c == '-' || c == '_' || c == ' ') {
            if(j > 0U && !previous_separator) {
                out[j++] = '_';
                previous_separator = true;
            }
        }
    }
    while(j > 0U && out[j - 1U] == '_') j--;
    out[j] = '\0';
}

static bool tpms_save_make_named_datetime_path(
    Storage* storage,
    const char* folder,
    const char* label,
    const char* extension,
    FuriString* path) {
    if(!storage || !folder || !extension || !path) return false;
    if(!label || !label[0]) return tpms_save_make_datetime_path(storage, folder, extension, path);

    char safe_label[32];
    tpms_save_sanitize_label(label, safe_label, sizeof(safe_label));
    if(!safe_label[0]) return tpms_save_make_datetime_path(storage, folder, extension, path);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(
        path,
        "%s/%s_%04u-%02u-%02u_%02u-%02u-%02u.%s",
        folder,
        safe_label,
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second,
        extension);

    for(uint16_t n = 2U; storage_file_exists(storage, furi_string_get_cstr(path)); n++) {
        furi_string_printf(
            path,
            "%s/%s_%04u-%02u-%02u_%02u-%02u-%02u_%u.%s",
            folder,
            safe_label,
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day,
            (unsigned)dt.hour,
            (unsigned)dt.minute,
            (unsigned)dt.second,
            n,
            extension);
        if(n == UINT16_MAX) return false;
    }
    return true;
}

static bool tpms_save_make_path(
    Storage* storage,
    const char* folder,
    const char* protocol,
    uint32_t id,
    const char* suffix,
    FuriString* path) {
    UNUSED(protocol);
    UNUSED(id);
    UNUSED(suffix);
    return tpms_save_make_datetime_path(storage, folder, "sub", path);
}

static bool tpms_raw_writer_flush_chunk(TpmsRawWriter* writer) {
    if(!writer || !writer->ok) return false;
    if(writer->count == 0U) return true;
    writer->ok = flipper_format_write_int32(
        writer->format, "RAW_Data", writer->chunk, writer->count);
    writer->count = 0U;
    return writer->ok;
}

static bool tpms_raw_writer_emit(TpmsRawWriter* writer, int32_t value) {
    if(!writer || !writer->ok || value == 0) return writer && writer->ok;
    writer->chunk[writer->count++] = value;
    if(writer->count >= TPMS_RAW_CHUNK) return tpms_raw_writer_flush_chunk(writer);
    return true;
}

static bool tpms_raw_writer_append(TpmsRawWriter* writer, int32_t value) {
    if(!writer || !writer->ok || value == 0) return writer && writer->ok;

    if(!writer->has_pending) {
        writer->pending = value;
        writer->has_pending = true;
        return true;
    }

    const bool same_sign = (writer->pending > 0 && value > 0) ||
                           (writer->pending < 0 && value < 0);
    if(same_sign) {
        int64_t merged = (int64_t)writer->pending + value;
        if(merged > INT32_MAX) merged = INT32_MAX;
        if(merged < INT32_MIN) merged = INT32_MIN;
        writer->pending = (int32_t)merged;
        return true;
    }

    if(!tpms_raw_writer_emit(writer, writer->pending)) return false;
    writer->pending = value;
    return true;
}

static bool tpms_raw_writer_finish(TpmsRawWriter* writer) {
    if(!writer || !writer->ok) return false;
    if(writer->has_pending) {
        if(!tpms_raw_writer_emit(writer, writer->pending)) return false;
        writer->has_pending = false;
        writer->pending = 0;
    }
    return tpms_raw_writer_flush_chunk(writer);
}

static bool tpms_save_write_header(
    FlipperFormat* format,
    uint32_t frequency,
    const char* preset_file_name,
    const uint8_t* custom_data,
    size_t custom_size) {
    if(!format || !preset_file_name) return false;
    if(!flipper_format_write_header_cstr(format, TPMS_RAW_FILE_TYPE, TPMS_RAW_FILE_VERSION))
        return false;
    if(!flipper_format_write_uint32(format, "Frequency", &frequency, 1U)) return false;
    if(!flipper_format_write_string_cstr(format, "Preset", preset_file_name)) return false;
    if(strcmp(preset_file_name, "FuriHalSubGhzPresetCustom") == 0) {
        if(!custom_data || custom_size == 0U) return false;
        if(!flipper_format_write_string_cstr(format, "Custom_preset_module", "CC1101"))
            return false;
        if(!flipper_format_write_hex(format, "Custom_preset_data", custom_data, custom_size))
            return false;
    }
    return flipper_format_write_string_cstr(format, "Protocol", "RAW");
}

static bool tpms_save_write_original_file(
    Storage* storage,
    const char* path,
    const SubGhzRadioPreset* preset,
    const int16_t* samples,
    uint16_t sample_count) {
    if(!storage || !path || !preset || !preset->name || !samples || sample_count == 0U)
        return false;

    FlipperFormat* format = flipper_format_file_alloc(storage);
    if(!format) return false;
    FuriString* preset_name = furi_string_alloc();
    bool opened = false;
    bool ok = false;

    do {
        opened = flipper_format_file_open_always(format, path);
        if(!opened) break;

        tpms_block_generic_get_preset_name(furi_string_get_cstr(preset->name), preset_name);
        if(!tpms_save_write_header(
               format,
               preset->frequency,
               furi_string_get_cstr(preset_name),
               preset->data,
               preset->data_size)) {
            break;
        }

        TpmsRawWriter writer = {
            .format = format,
            .count = 0U,
            .pending = 0,
            .has_pending = false,
            .ok = true,
        };
        for(uint16_t i = 0U; i < sample_count && writer.ok; i++) {
            (void)tpms_raw_writer_append(&writer, samples[i]);
        }
        ok = tpms_raw_writer_finish(&writer);
    } while(false);

    if(opened) flipper_format_file_close(format);
    flipper_format_free(format);
    furi_string_free(preset_name);
    return ok;
}

static const uint8_t* tpms_save_preset_data_for_wave(
    const TpmsTxWave* wave,
    size_t* preset_size) {
    if(!wave || !preset_size) return NULL;
    if(wave->modulation == OOK_PULSE_PCM ||
       wave->modulation == OOK_PULSE_MANCHESTER_ZEROBIT) {
        *preset_size = protoview_subghz_tpms2_ook_async_regs_size;
        return (const uint8_t*)protoview_subghz_tpms2_ook_async_regs;
    }
    *preset_size = protoview_subghz_tpms1_fsk_async_regs_size;
    return (const uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
}

static bool tpms_save_write_edited_file(
    Storage* storage,
    const char* path,
    const TpmsEditValues* values,
    const TpmsTxWave* wave) {
    if(!storage || !path || !values || !wave || wave->pulse_count == 0U) return false;

    size_t preset_size = 0U;
    const uint8_t* preset_data = tpms_save_preset_data_for_wave(wave, &preset_size);
    if(!preset_data || preset_size == 0U) return false;

    FlipperFormat* format = flipper_format_file_alloc(storage);
    if(!format) return false;
    bool opened = false;
    bool ok = false;

    do {
        opened = flipper_format_file_open_always(format, path);
        if(!opened) break;
        if(!tpms_save_write_header(
               format,
               wave->frequency_hz,
               "FuriHalSubGhzPresetCustom",
               preset_data,
               preset_size)) {
            break;
        }

        uint8_t repeat = values->repeat_count;
        if(repeat == 0U) repeat = 1U;
        if(repeat > 10U) repeat = 10U;

        TpmsRawWriter writer = {
            .format = format,
            .count = 0U,
            .pending = 0,
            .has_pending = false,
            .ok = true,
        };

        for(uint8_t r = 0U; r < repeat && writer.ok; r++) {
            for(size_t i = 0U; i < wave->pulse_count && writer.ok; i++) {
                uint32_t duration = wave->pulses[i].duration_us;
                if(duration > INT32_MAX) duration = INT32_MAX;
                const int32_t signed_duration = wave->pulses[i].level ?
                                                    (int32_t)duration :
                                                    -(int32_t)duration;
                (void)tpms_raw_writer_append(&writer, signed_duration);
            }
            if(r + 1U < repeat) {
                (void)tpms_raw_writer_append(&writer, -(int32_t)TPMS_REPEAT_GAP_US);
            }
        }
        ok = tpms_raw_writer_finish(&writer);
    } while(false);

    if(opened) flipper_format_file_close(format);
    flipper_format_free(format);
    return ok;
}

bool tpms_profile_save_named(
    const TpmsEditValues* values,
    const char* protocol,
    bool edited,
    const char* label,
    FuriString* saved_path) {
    if(saved_path) furi_string_reset(saved_path);
    if(!values || !protocol || !protocol[0]) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_PROFILE_FOLDER);
    storage_common_mkdir(storage, TPMS_PROFILE_RX_FOLDER);
    storage_common_mkdir(storage, TPMS_PROFILE_EDITED_FOLDER);
    const char* profile_folder = edited ? TPMS_PROFILE_EDITED_FOLDER : TPMS_PROFILE_RX_FOLDER;

    FuriString* path = saved_path ? saved_path : furi_string_alloc();
    if(!path) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool ok = false;
    if(tpms_save_make_named_datetime_path(storage, profile_folder, label, "tpms", path)) {
        FlipperFormat* format = flipper_format_file_alloc(storage);
        if(format) {
            bool opened = flipper_format_file_open_always(format, furi_string_get_cstr(path));
            if(opened) {
                uint32_t u32;
                float f32;
                ok = flipper_format_write_header_cstr(format, "TPMS Editor Profile", 3U);
                if(ok) ok = flipper_format_write_string_cstr(format, "Protocol", protocol);
                if(ok && label && label[0])
                    ok = flipper_format_write_string_cstr(format, "Label", label);
                u32 = (uint32_t)values->kind;
                if(ok) ok = flipper_format_write_uint32(format, "Kind", &u32, 1U);
                u32 = values->id;
                if(ok) ok = flipper_format_write_uint32(format, "Id", &u32, 1U);
                f32 = values->pressure_kpa;
                if(ok) ok = flipper_format_write_float(format, "PressureKPa", &f32, 1U);
                f32 = values->temperature_c;
                if(ok) ok = flipper_format_write_float(format, "TemperatureC", &f32, 1U);
                u32 = values->flags;
                if(ok) ok = flipper_format_write_uint32(format, "Flags", &u32, 1U);
                u32 = values->temperature_raw_f;
                if(ok) ok = flipper_format_write_uint32(format, "TemperatureRawF", &u32, 1U);
                u32 = values->temperature_raw_f_valid ? 1U : 0U;
                if(ok) ok = flipper_format_write_uint32(format, "TemperatureRawFValid", &u32, 1U);
                u32 = values->frequency_hz;
                if(ok) ok = flipper_format_write_uint32(format, "Frequency", &u32, 1U);
                u32 = values->radio_preset;
                if(ok) ok = flipper_format_write_uint32(format, "RadioPreset", &u32, 1U);
                if(ok && values->rssi_valid) {
                    int32_t rssi = values->rssi_dbm;
                    ok = flipper_format_write_int32(format, "RSSI_dBm", &rssi, 1U);
                }
                if(ok)
                    ok = flipper_format_write_string_cstr(
                        format, "RxPreset", tpms_radio_preset_name(values->radio_preset));
                if(ok)
                    ok = flipper_format_write_string_cstr(
                        format, "Modulation", tpms_radio_preset_name(values->radio_preset));
                if(ok && values->wake_profile_valid) {
                    u32 = values->wake_profile;
                    ok = flipper_format_write_uint32(format, "WakeProfile", &u32, 1U);
                }
                if(ok && values->wake_profile_valid)
                    ok = flipper_format_write_string_cstr(
                        format, "WakeProfileName", tpms_wake_profile_name(values->wake_profile));
                u32 = values->repeat_count;
                if(ok) ok = flipper_format_write_uint32(format, "Repeat", &u32, 1U);
                u32 = values->edit_mask;
                if(ok) ok = flipper_format_write_uint32(format, "EditMask", &u32, 1U);
                u32 = values->raw_payload_valid ? values->raw_payload_size : 0U;
                if(ok) ok = flipper_format_write_uint32(format, "RawPayloadSize", &u32, 1U);
                u32 = values->raw_payload_valid ? values->raw_payload_bits : 0U;
                if(ok) ok = flipper_format_write_uint32(format, "RawPayloadBits", &u32, 1U);
                if(ok && values->raw_payload_valid && values->raw_payload_size > 0U &&
                   values->raw_payload_size <= sizeof(values->raw_payload)) {
                    ok = flipper_format_write_hex(
                        format, "RawPayload", values->raw_payload, values->raw_payload_size);
                }
                if(ok) ok = tpms_profile_write_ford_diagnostics(format, values);
                flipper_format_file_close(format);
            }
            flipper_format_free(format);
        }
    }

    if(!saved_path) furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tpms_profile_save(
    const TpmsEditValues* values,
    const char* protocol,
    bool edited,
    FuriString* saved_path) {
    return tpms_profile_save_named(values, protocol, edited, NULL, saved_path);
}

bool tpms_profile_read_label(
    const char* path,
    char* label,
    size_t label_size,
    uint32_t* id) {
    if(!path || !label || label_size < 2U) return false;
    label[0] = '\0';
    if(id) *id = 0U;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    FlipperFormat* format = flipper_format_file_alloc(storage);
    FuriString* text = furi_string_alloc();
    bool opened = false;
    bool ok = false;
    if(format && text) {
        opened = flipper_format_file_open_existing(format, path);
        if(opened) {
            if(flipper_format_read_string(format, "Label", text)) {
                snprintf(label, label_size, "%s", furi_string_get_cstr(text));
                ok = label[0] != '\0';
            }
            if(id) {
                uint32_t value = 0U;
                if(flipper_format_rewind(format) &&
                   flipper_format_read_uint32(format, "Id", &value, 1U)) {
                    *id = value;
                }
            }
        }
    }
    if(opened) flipper_format_file_close(format);
    if(text) furi_string_free(text);
    if(format) flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tpms_profile_load(
    const char* path,
    TpmsEditValues* values,
    char* protocol,
    size_t protocol_size) {
    if(!path || !values || !protocol || protocol_size < 2U) return false;
    protocol[0] = '\0';

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    FlipperFormat* format = flipper_format_file_alloc(storage);
    if(!format) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    FuriString* proto = furi_string_alloc();
    bool opened = false;
    bool ok = false;
    do {
        opened = flipper_format_file_open_existing(format, path);
        if(!opened) break;
        if(!flipper_format_read_string(format, "Protocol", proto)) break;
        snprintf(protocol, protocol_size, "%s", furi_string_get_cstr(proto));

        uint32_t u32 = 0U;
        float f32 = 0.0f;
        if(!flipper_format_read_uint32(format, "Kind", &u32, 1U)) break;
        values->kind = (u32 < TpmsEncoderCount) ? (TpmsEncoderKind)u32 : TpmsEncoderNone;
        if(!flipper_format_read_uint32(format, "Id", &values->id, 1U)) break;
        if(!flipper_format_read_float(format, "PressureKPa", &f32, 1U)) break;
        values->pressure_kpa = f32;
        if(!flipper_format_read_float(format, "TemperatureC", &f32, 1U)) break;
        values->temperature_c = f32;
        if(!flipper_format_read_uint32(format, "Flags", &u32, 1U)) break;
        values->flags = u32;
        values->temperature_raw_f = 0U;
        values->temperature_raw_f_valid = false;
        if(flipper_format_rewind(format)) {
            uint32_t raw_f = 0U;
            if(flipper_format_read_uint32(format, "TemperatureRawF", &raw_f, 1U)) {
                values->temperature_raw_f = (uint8_t)raw_f;
            }
        }
        if(flipper_format_rewind(format)) {
            uint32_t raw_valid = 0U;
            if(flipper_format_read_uint32(format, "TemperatureRawFValid", &raw_valid, 1U)) {
                values->temperature_raw_f_valid = raw_valid != 0U;
            }
        }
        if(!flipper_format_rewind(format)) break;
        if(!flipper_format_read_uint32(format, "Frequency", &values->frequency_hz, 1U)) break;
        values->radio_preset = TpmsRadioPresetAuto;
        if(flipper_format_rewind(format)) {
            uint32_t radio_preset = TpmsRadioPresetAuto;
            if(flipper_format_read_uint32(format, "RadioPreset", &radio_preset, 1U) &&
               radio_preset <= TpmsRadioPresetGFSK) {
                values->radio_preset = (uint8_t)radio_preset;
            }
        }
        values->rssi_dbm = -127;
        values->rssi_valid = false;
        if(flipper_format_rewind(format)) {
            int32_t rssi = -127;
            if(flipper_format_read_int32(format, "RSSI_dBm", &rssi, 1U) &&
               rssi >= -127 && rssi <= 20) {
                values->rssi_dbm = (int16_t)rssi;
                values->rssi_valid = true;
            }
        }
        values->wake_profile = TpmsWakeProfileNone;
        values->wake_profile_valid = false;
        if(flipper_format_rewind(format)) {
            uint32_t wake_profile = TpmsWakeProfileNone;
            if(flipper_format_read_uint32(format, "WakeProfile", &wake_profile, 1U) &&
               wake_profile <= TpmsWakeProfileAutoFordVDO &&
               wake_profile != TpmsWakeProfileNone) {
                values->wake_profile = (uint8_t)wake_profile;
                values->wake_profile_valid = true;
            }
        }
        if(!flipper_format_rewind(format)) break;
        if(!flipper_format_read_uint32(format, "Repeat", &u32, 1U)) break;
        values->repeat_count = (uint8_t)u32;
        if(values->repeat_count == 0U) values->repeat_count = 3U;

        /* Profile v3 stores which logical fields were actually edited.
         * Legacy profiles did not, so preserve their old behavior by
         * reapplying all stored values. */
        values->edit_mask = TPMS_EDIT_ALL;
        if(flipper_format_rewind(format)) {
            uint32_t edit_mask = TPMS_EDIT_ALL;
            if(flipper_format_read_uint32(format, "EditMask", &edit_mask, 1U)) {
                values->edit_mask = (uint8_t)(edit_mask & TPMS_EDIT_ALL);
            }
        }

        values->raw_payload_size = 0U;
        values->raw_payload_bits = 0U;
        values->raw_payload_valid = false;
        if(flipper_format_rewind(format)) {
            uint32_t raw_size = 0U;
            if(flipper_format_read_uint32(format, "RawPayloadSize", &raw_size, 1U) &&
               raw_size > 0U && raw_size <= sizeof(values->raw_payload)) {
                uint32_t raw_bits = raw_size * 8U;
                if(flipper_format_rewind(format)) {
                    (void)flipper_format_read_uint32(format, "RawPayloadBits", &raw_bits, 1U);
                }
                if(flipper_format_rewind(format) &&
                   flipper_format_read_hex(format, "RawPayload", values->raw_payload, raw_size)) {
                    values->raw_payload_size = (uint8_t)raw_size;
                    values->raw_payload_bits = (uint8_t)(raw_bits > TPMS_RAWBITS_BUF ? TPMS_RAWBITS_BUF : raw_bits);
                    values->raw_payload_valid = true;
                }
            }
        }

        if(values->kind == TpmsEncoderNone) values->kind = tpms_encoder_for_model(protocol);
        ok = values->kind != TpmsEncoderNone;
    } while(false);

    if(opened) flipper_format_file_close(format);
    furi_string_free(proto);
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tpms_save_original_raw_named(
    TPMSApp* app,
    uint16_t history_index,
    const char* protocol,
    uint32_t id,
    const char* label,
    FuriString* saved_path,
    bool* truncated) {
    UNUSED(protocol);
    UNUSED(id);
    if(saved_path) furi_string_reset(saved_path);
    if(truncated) *truncated = false;
    if(!app || !app->txrx || !app->txrx->history) return false;

    int16_t* samples = malloc(TPMS_CAPTURE_MAX_SAMPLES * sizeof(int16_t));
    if(!samples) return false;

    uint16_t sample_count = 0U;
    bool capture_truncated = false;
    SubGhzRadioPreset preset_copy = {0};
    FuriString* preset_name_copy = furi_string_alloc();
    bool copied = false;

    furi_mutex_acquire(app->history_mutex, FuriWaitForever);
    if(history_index < tpms_history_get_item(app->txrx->history)) {
        SubGhzRadioPreset* preset =
            tpms_history_get_radio_preset(app->txrx->history, history_index);
        copied = tpms_history_copy_raw_capture(
            app->txrx->history,
            history_index,
            samples,
            TPMS_CAPTURE_MAX_SAMPLES,
            &sample_count,
            &capture_truncated);
        if(copied && preset && preset->name) {
            preset_copy = *preset;
            furi_string_set(preset_name_copy, preset->name);
            preset_copy.name = preset_name_copy;
        } else {
            copied = false;
        }
    }
    furi_mutex_release(app->history_mutex);

    if(!copied || sample_count == 0U) {
        furi_string_free(preset_name_copy);
        free(samples);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        furi_string_free(preset_name_copy);
        free(samples);
        return false;
    }
    storage_common_mkdir(storage, EXT_PATH("subghz"));
    storage_common_mkdir(storage, TPMS_SAVE_FOLDER);
    storage_common_mkdir(storage, TPMS_SAVE_FOLDER_ORG);

    FuriString* path = saved_path ? saved_path : furi_string_alloc();
    bool ok = false;
    if(tpms_save_make_named_datetime_path(storage, TPMS_SAVE_FOLDER_ORG, label, "sub", path)) {
        ok = tpms_save_write_original_file(
            storage,
            furi_string_get_cstr(path),
            &preset_copy,
            samples,
            sample_count);
    }

    if(!saved_path) furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(preset_name_copy);
    free(samples);
    if(truncated) *truncated = capture_truncated;
    return ok;
}

bool tpms_save_original_raw(
    TPMSApp* app,
    uint16_t history_index,
    const char* protocol,
    uint32_t id,
    FuriString* saved_path,
    bool* truncated) {
    return tpms_save_original_raw_named(
        app, history_index, protocol, id, NULL, saved_path, truncated);
}

bool tpms_save_edited_raw(
    TPMSApp* app,
    const TpmsEditValues* values,
    const char* protocol,
    FuriString* saved_path) {
    UNUSED(app);
    if(saved_path) furi_string_reset(saved_path);
    if(!values || values->kind == TpmsEncoderNone) return false;

    TpmsTxWave* wave = malloc(sizeof(TpmsTxWave));
    if(!wave) return false;
    if(!tpms_encoder_build(values, wave) || wave->pulse_count == 0U || wave->overflowed) {
        free(wave);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        free(wave);
        return false;
    }
    storage_common_mkdir(storage, EXT_PATH("subghz"));
    storage_common_mkdir(storage, TPMS_SAVE_FOLDER);
    storage_common_mkdir(storage, TPMS_SAVE_FOLDER_MOD);

    FuriString* path = saved_path ? saved_path : furi_string_alloc();
    bool ok = false;
    if(tpms_save_make_path(storage, TPMS_SAVE_FOLDER_MOD, protocol, values->id, "EDIT", path)) {
        ok = tpms_save_write_edited_file(
            storage, furi_string_get_cstr(path), values, wave);
    }

    if(!saved_path) furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    free(wave);
    if(ok) ok = tpms_profile_save(values, protocol, true, NULL);
    return ok;
}


bool tpms_profile_save_to_path_named(
    const char* path,
    const TpmsEditValues* values,
    const char* protocol,
    const char* label) {
    if(!path || !path[0] || !values || !protocol || !protocol[0]) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    FlipperFormat* format = flipper_format_file_alloc(storage);
    bool ok = false;
    if(format && flipper_format_file_open_always(format, path)) {
        uint32_t u32;
        float f32;
        ok = flipper_format_write_header_cstr(format, "TPMS Editor Profile", 3U);
        if(ok) ok = flipper_format_write_string_cstr(format, "Protocol", protocol);
        if(ok && label && label[0])
            ok = flipper_format_write_string_cstr(format, "Label", label);
        u32 = (uint32_t)values->kind;
        if(ok) ok = flipper_format_write_uint32(format, "Kind", &u32, 1U);
        u32 = values->id;
        if(ok) ok = flipper_format_write_uint32(format, "Id", &u32, 1U);
        f32 = values->pressure_kpa;
        if(ok) ok = flipper_format_write_float(format, "PressureKPa", &f32, 1U);
        f32 = values->temperature_c;
        if(ok) ok = flipper_format_write_float(format, "TemperatureC", &f32, 1U);
        u32 = values->flags;
        if(ok) ok = flipper_format_write_uint32(format, "Flags", &u32, 1U);
        u32 = values->temperature_raw_f;
        if(ok) ok = flipper_format_write_uint32(format, "TemperatureRawF", &u32, 1U);
        u32 = values->temperature_raw_f_valid ? 1U : 0U;
        if(ok) ok = flipper_format_write_uint32(format, "TemperatureRawFValid", &u32, 1U);
        u32 = values->frequency_hz;
        if(ok) ok = flipper_format_write_uint32(format, "Frequency", &u32, 1U);
        u32 = values->radio_preset;
        if(ok) ok = flipper_format_write_uint32(format, "RadioPreset", &u32, 1U);
        if(ok && values->rssi_valid) {
            int32_t rssi = values->rssi_dbm;
            ok = flipper_format_write_int32(format, "RSSI_dBm", &rssi, 1U);
        }
        if(ok)
            ok = flipper_format_write_string_cstr(
                format, "RxPreset", tpms_radio_preset_name(values->radio_preset));
        if(ok)
            ok = flipper_format_write_string_cstr(
                format, "Modulation", tpms_radio_preset_name(values->radio_preset));
        if(ok && values->wake_profile_valid) {
            u32 = values->wake_profile;
            ok = flipper_format_write_uint32(format, "WakeProfile", &u32, 1U);
        }
        if(ok && values->wake_profile_valid)
            ok = flipper_format_write_string_cstr(
                format, "WakeProfileName", tpms_wake_profile_name(values->wake_profile));
        u32 = values->repeat_count;
        if(ok) ok = flipper_format_write_uint32(format, "Repeat", &u32, 1U);
        u32 = values->edit_mask;
        if(ok) ok = flipper_format_write_uint32(format, "EditMask", &u32, 1U);
        u32 = values->raw_payload_valid ? values->raw_payload_size : 0U;
        if(ok) ok = flipper_format_write_uint32(format, "RawPayloadSize", &u32, 1U);
        u32 = values->raw_payload_valid ? values->raw_payload_bits : 0U;
        if(ok) ok = flipper_format_write_uint32(format, "RawPayloadBits", &u32, 1U);
        if(ok && values->raw_payload_valid && values->raw_payload_size > 0U &&
           values->raw_payload_size <= sizeof(values->raw_payload)) {
            ok = flipper_format_write_hex(
                format, "RawPayload", values->raw_payload, values->raw_payload_size);
        }
        if(ok) ok = tpms_profile_write_ford_diagnostics(format, values);
        flipper_format_file_close(format);
    }
    if(format) flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tpms_profile_save_to_path(
    const char* path, const TpmsEditValues* values, const char* protocol) {
    return tpms_profile_save_to_path_named(path, values, protocol, NULL);
}


/* ---- TPMS 4.5 automatic RX profile capture -------------------------------- */

#define TPMS_AUTOSAVE_CONFIG_TYPE "TPMS Autosave Config"
#define TPMS_AUTOSAVE_CONFIG_VERSION 1U
#define TPMS_AUTOSAVE_CACHE_SIZE 32U

static uint32_t tpms_autosave_fnv1a(const void* data, size_t size, uint32_t seed) {
    const uint8_t* bytes = data;
    uint32_t hash = seed ? seed : UINT32_C(2166136261);
    for(size_t i = 0U; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

static uint32_t tpms_autosave_key(const char* protocol, uint32_t id) {
    uint32_t hash = tpms_autosave_fnv1a(protocol, strlen(protocol), 0U);
    return tpms_autosave_fnv1a(&id, sizeof(id), hash);
}

static uint32_t tpms_autosave_payload_hash(const TPMSBlockGeneric* generic) {
    if(!generic) return 0U;
    uint32_t hash = UINT32_C(2166136261);
    if(generic->raw_payload_valid && generic->raw_payload_size > 0U) {
        hash = tpms_autosave_fnv1a(
            generic->raw_payload, generic->raw_payload_size, hash);
        hash = tpms_autosave_fnv1a(
            &generic->raw_payload_bits, sizeof(generic->raw_payload_bits), hash);
        return hash;
    }

    hash = tpms_autosave_fnv1a(&generic->data, sizeof(generic->data), hash);
    hash = tpms_autosave_fnv1a(&generic->pressure, sizeof(generic->pressure), hash);
    hash = tpms_autosave_fnv1a(&generic->temperature, sizeof(generic->temperature), hash);
    return hash;
}

static bool tpms_autosave_is_duplicate(
    const TPMSApp* app, uint32_t key, uint32_t payload_hash) {
    if(!app) return false;
    for(uint8_t i = 0U; i < app->autosave_cache_count; i++) {
        if(app->autosave_cache_keys[i] == key &&
           app->autosave_cache_hashes[i] == payload_hash) {
            return true;
        }
    }
    return false;
}

static void tpms_autosave_remember(
    TPMSApp* app, uint32_t key, uint32_t payload_hash) {
    if(!app) return;

    /* Update an existing sensor slot first. A changed state for the same
       protocol+ID replaces only its last payload signature. */
    for(uint8_t i = 0U; i < app->autosave_cache_count; i++) {
        if(app->autosave_cache_keys[i] == key) {
            app->autosave_cache_hashes[i] = payload_hash;
            return;
        }
    }

    if(app->autosave_cache_count < TPMS_AUTOSAVE_CACHE_SIZE) {
        const uint8_t idx = app->autosave_cache_count++;
        app->autosave_cache_keys[idx] = key;
        app->autosave_cache_hashes[idx] = payload_hash;
        return;
    }

    /* More than 32 distinct sensors in one session: replace oldest cache
       slots round-robin. Files already saved on SD remain untouched. */
    const uint8_t idx = app->autosave_cache_next % TPMS_AUTOSAVE_CACHE_SIZE;
    app->autosave_cache_keys[idx] = key;
    app->autosave_cache_hashes[idx] = payload_hash;
    app->autosave_cache_next =
        (uint8_t)((idx + 1U) % TPMS_AUTOSAVE_CACHE_SIZE);
}

void tpms_autosave_reset_session(TPMSApp* app) {
    if(!app) return;
    app->autosave_saved_count = 0U;
    app->autosave_error_count = 0U;
    app->autosave_cache_count = 0U;
    app->autosave_cache_next = 0U;
    memset(app->autosave_cache_keys, 0, sizeof(app->autosave_cache_keys));
    memset(app->autosave_cache_hashes, 0, sizeof(app->autosave_cache_hashes));
}

bool tpms_autosave_load_enabled(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return TPMS_AUTOSAVE_DEFAULT_ENABLED;

    FlipperFormat* format = flipper_format_file_alloc(storage);
    FuriString* type = furi_string_alloc();
    uint32_t version = 0U;
    uint32_t enabled = 0U;
    /* FIX2: a fresh installation starts with RX Autosave enabled. A valid
       persisted Enabled=0 remains authoritative after the user turns it off. */
    bool result = TPMS_AUTOSAVE_DEFAULT_ENABLED;

    if(format && type &&
       flipper_format_file_open_existing(format, TPMS_AUTOSAVE_CONFIG)) {
        if(flipper_format_read_header(format, type, &version) &&
           strcmp(furi_string_get_cstr(type), TPMS_AUTOSAVE_CONFIG_TYPE) == 0 &&
           version == TPMS_AUTOSAVE_CONFIG_VERSION &&
           flipper_format_read_uint32(format, "Enabled", &enabled, 1U)) {
            result = enabled != 0U;
        }
        flipper_format_file_close(format);
    }

    if(type) furi_string_free(type);
    if(format) flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool tpms_autosave_set_enabled(TPMSApp* app, bool enabled) {
    if(!app) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_AUTOSAVE_FOLDER);

    FlipperFormat* format = flipper_format_file_alloc(storage);
    bool ok = false;
    if(format && flipper_format_file_open_always(format, TPMS_AUTOSAVE_CONFIG)) {
        uint32_t value = enabled ? 1U : 0U;
        ok = flipper_format_write_header_cstr(
            format, TPMS_AUTOSAVE_CONFIG_TYPE, TPMS_AUTOSAVE_CONFIG_VERSION);
        if(ok) ok = flipper_format_write_uint32(format, "Enabled", &value, 1U);
        flipper_format_file_close(format);
    }
    if(format) flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        app->autosave_enabled = enabled;
        if(enabled) tpms_autosave_reset_session(app);
    }
    return ok;
}

static void tpms_autosave_type_token(const char* protocol, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    const char* display = tpms_protocol_list_name(protocol);
    if(!display || !display[0]) display = protocol;
    size_t j = 0U;
    bool separator = false;
    for(size_t i = 0U; display && display[i] && j + 1U < out_size; i++) {
        const unsigned char c = (unsigned char)display[i];
        if(isalnum(c)) {
            out[j++] = (char)toupper(c);
            separator = false;
        } else if(j > 0U && !separator) {
            out[j++] = '_';
            separator = true;
        }
    }
    while(j > 0U && out[j - 1U] == '_') j--;
    out[j] = '\0';
    if(!out[0]) snprintf(out, out_size, "TPMS");
}

bool tpms_autosave_process_decoder(
    TPMSApp* app,
    void* decoder_base_ptr,
    float frame_rssi,
    uint8_t wake_profile,
    bool wake_profile_valid) {
    if(!app || !app->autosave_enabled || !decoder_base_ptr || !app->txrx ||
       !app->txrx->preset) {
        return false;
    }

    SubGhzProtocolDecoderBase* decoder_base = decoder_base_ptr;
    FlipperFormat* serialized = flipper_format_string_alloc();
    FuriString* protocol = furi_string_alloc();
    TPMSBlockGeneric generic = {0};
    TpmsEditValues values = {0};
    bool saved = false;

    do {
        if(!serialized || !protocol) break;
        if(subghz_protocol_decoder_base_serialize(
               decoder_base, serialized, app->txrx->preset) !=
           SubGhzProtocolStatusOk) {
            break;
        }
        if(!flipper_format_rewind(serialized) ||
           !flipper_format_read_string(serialized, "Protocol", protocol)) {
            break;
        }
        if(tpms_block_generic_deserialize(&generic, serialized) !=
           SubGhzProtocolStatusOk) {
            break;
        }

        const char* protocol_cstr = furi_string_get_cstr(protocol);
        const uint32_t key = tpms_autosave_key(protocol_cstr, generic.id);
        const uint8_t rx_preset =
            tpms_radio_preset_from_name(furi_string_get_cstr(app->txrx->preset->name));
        uint32_t payload_hash = tpms_autosave_payload_hash(&generic);
        payload_hash = tpms_autosave_fnv1a(
            &app->txrx->preset->frequency, sizeof(app->txrx->preset->frequency), payload_hash);
        payload_hash = tpms_autosave_fnv1a(&rx_preset, sizeof(rx_preset), payload_hash);
        const uint8_t wake_signature =
            wake_profile_valid ? wake_profile : TpmsWakeProfileNone;
        payload_hash =
            tpms_autosave_fnv1a(&wake_signature, sizeof(wake_signature), payload_hash);
        if(tpms_autosave_is_duplicate(app, key, payload_hash)) break;

        values.kind = tpms_encoder_for_model(protocol_cstr);
        values.id = generic.id;
        values.pressure_kpa = generic.pressure * 100.0f;
        values.temperature_c = generic.temperature;
        values.frequency_hz = app->txrx->preset->frequency;
        values.radio_preset = rx_preset;
        values.rssi_dbm =
            (int16_t)(frame_rssi < 0.0f ? frame_rssi - 0.5f : frame_rssi + 0.5f);
        values.rssi_valid = frame_rssi > -127.0f && frame_rssi <= 20.0f;
        values.wake_profile = wake_profile_valid ? wake_profile : TpmsWakeProfileNone;
        values.wake_profile_valid =
            wake_profile_valid && wake_profile != TpmsWakeProfileNone;
        values.repeat_count = 3U;
        values.edit_mask = 0U;

        if(values.kind == TpmsEncoderSchraderEG53MA4) {
            values.flags = (uint32_t)(generic.data >> 32U);
            values.temperature_raw_f = (uint8_t)((generic.data >> 24U) & 0xFFU);
            values.temperature_raw_f_valid = true;
            values.temperature_c =
                ((float)values.temperature_raw_f - 32.0f) * (5.0f / 9.0f);
        } else {
            values.flags = (uint32_t)((generic.data >> 48U) & 0xFFFFU);
            values.temperature_raw_f = 0U;
            values.temperature_raw_f_valid = false;
        }

        values.raw_payload_size = generic.raw_payload_size;
        values.raw_payload_bits = generic.raw_payload_bits;
        values.raw_payload_valid = generic.raw_payload_valid;
        if(generic.raw_payload_valid && generic.raw_payload_size > 0U &&
           generic.raw_payload_size <= sizeof(values.raw_payload)) {
            memcpy(values.raw_payload, generic.raw_payload, generic.raw_payload_size);
        }

        Storage* storage = furi_record_open(RECORD_STORAGE);
        if(!storage) break;
        storage_common_mkdir(storage, EXT_PATH("apps_data"));
        storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
        storage_common_mkdir(storage, TPMS_AUTOSAVE_FOLDER);

        char type_token[24];
        char prefix[40];
        tpms_autosave_type_token(protocol_cstr, type_token, sizeof(type_token));
        snprintf(
            prefix,
            sizeof(prefix),
            "%s_%08lX",
            type_token,
            (unsigned long)generic.id);

        FuriString* path = furi_string_alloc();
        const bool path_ok =
            path &&
            tpms_save_make_named_datetime_path(
                storage, TPMS_AUTOSAVE_FOLDER, prefix, "tpms", path);
        furi_record_close(RECORD_STORAGE);

        if(path_ok) {
            saved = tpms_profile_save_to_path(
                furi_string_get_cstr(path), &values, protocol_cstr);
        }
        if(path) furi_string_free(path);

        if(saved) {
            tpms_autosave_remember(app, key, payload_hash);
            app->autosave_saved_count++;
        } else {
            app->autosave_error_count++;
        }
    } while(false);

    if(protocol) furi_string_free(protocol);
    if(serialized) flipper_format_free(serialized);
    return saved;
}
