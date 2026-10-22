#include <furi.h>
#include <flipper_format/flipper_format_i.h>
#include <storage/storage.h>
#include <lib/subghz/transmitter.h>

#include "../app_config.h"
#include "subghz_signal.h"

struct SubGhzSignal {
    FuriString* sub_file_contents;
    FuriString* protocol;
    FuriString* file_path;
    uint32_t frequency;
    FuriHalSubGhzPreset preset;
    uint8_t* preset_data;
};

#ifdef FIRMWARE_SUPPORTS_SUBGHZ
#include <devices/devices.h>
#ifdef APP_USES_SUBGHZ_PROTOCOL_ITEMS
#include <subghz_protocol_registry.h>
#endif
//#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
//#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#define SUBGHZ_DEVICE_CC1101_EXT_NAME "cc1101_ext"
#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"

typedef enum {
    Unknown,
    Raw,
    Key,
} FileType;

/**
 * @brief Turns on the +5V rail to the external radio.
*/
static void subghz_radio_device_power_on() {
    uint8_t attempts = 5;
    while(--attempts > 0) {
        if(furi_hal_power_enable_otg()) break;
        furi_delay_ms(50);
    }
}

/**
 * @brief Turns off the +5V rail to the external radio.
*/
static void subghz_radio_device_power_off() {
    if(furi_hal_power_is_otg_enabled()) furi_hal_power_disable_otg();
}

/**
 * @brief Populates a flipper_format with the contents of a FuriString.
 * @param flipper_format The flipper_format to populate.
 * @param contents The FuriString to populate the flipper_format with.
*/
static void __flipper_format_populate(FlipperFormat* flipper_format, FuriString* contents) {
    Stream* stream = flipper_format_get_raw_stream(flipper_format);
    stream_clean(stream);
    stream_write_cstring(stream, furi_string_get_cstr(contents));
    stream_seek(stream, 0, StreamOffsetFromStart);
}

/* Only .pending files contain fully written, synced state. A .tmp file is never
 * recovered. Keep the old file as .bak until the replacement has been installed. */
static bool subghz_signal_recover(Storage* storage, const char* path) {
    FuriString* pending = furi_string_alloc_printf("%s.flip.pending", path);
    FuriString* backup = furi_string_alloc_printf("%s.flip.bak", path);
    const char* p = furi_string_get_cstr(pending);
    const char* b = furi_string_get_cstr(backup);
    bool ok = false;
    do {
        if(storage_file_exists(storage, p)) {
            if(storage_file_exists(storage, path)) {
                if(storage_file_exists(storage, b)) break; // Ambiguous; preserve all copies.
                if(storage_common_rename_safe(storage, path, b) != FSE_OK) break;
            }
            if(storage_common_rename_safe(storage, p, path) != FSE_OK) break;
        } else if(!storage_file_exists(storage, path) && storage_file_exists(storage, b)) {
            if(storage_common_rename_safe(storage, b, path) != FSE_OK) break;
        }
        if(storage_file_exists(storage, b) && storage_common_remove(storage, b) != FSE_OK) break;
        ok = true;
    } while(false);
    furi_string_free(backup);
    furi_string_free(pending);
    return ok;
}

static bool subghz_signal_stage(FlipperFormat* format, Storage* storage, const char* path) {
    FuriString* temporary = furi_string_alloc_printf("%s.flip.tmp", path);
    FuriString* pending = furi_string_alloc_printf("%s.flip.pending", path);
    File* file = storage_file_alloc(storage);
    Stream* stream = flipper_format_get_raw_stream(format);
    bool ok = false;
    if(storage_file_open(file, furi_string_get_cstr(temporary), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t remaining = stream_size(stream);
        ok = remaining && stream_seek(stream, 0, StreamOffsetFromStart);
        uint8_t buffer[256];
        while(ok && remaining) {
            size_t count = MIN(remaining, sizeof(buffer));
            ok = stream_read(stream, buffer, count) == count &&
                 storage_file_write(file, buffer, count) == count;
            remaining -= count;
        }
        if(ok) ok = storage_file_sync(file);
        if(!storage_file_close(file)) ok = false;
        if(ok)
            ok = storage_common_rename_safe(
                     storage, furi_string_get_cstr(temporary), furi_string_get_cstr(pending)) ==
                 FSE_OK;
    }
    storage_file_free(file);
    furi_string_free(pending);
    furi_string_free(temporary);
    return ok;
}

typedef struct {
    SubGhzTransmitter* transmitter;
    uint32_t queued_until; // Written only by the DMA feeder.
    uint32_t tick_fraction;
    volatile uint32_t deadline; // One aligned word shared with the worker.
} SubGhzSignalFeed;

static LevelDuration subghz_signal_yield(void* context) {
    SubGhzSignalFeed* feed = context;
    LevelDuration sample = subghz_transmitter_yield(feed->transmitter);
    if(!level_duration_is_reset(sample) && !level_duration_is_wait(sample)) {
        uint32_t now = furi_get_tick();
        if((int32_t)(now - feed->queued_until) > 0) {
            feed->queued_until = now;
            feed->tick_fraction = 0;
        }
        uint64_t ticks =
            (uint64_t)level_duration_get_duration(sample) * furi_kernel_get_tick_frequency() +
            feed->tick_fraction;
        feed->queued_until += ticks / 1000000;
        feed->tick_fraction = ticks % 1000000;
        feed->deadline = feed->queued_until + furi_ms_to_ticks(5000) + 1;
    }
    return sample;
}

static bool subghz_signal_cancelled(uint32_t cancel_flag) {
    return cancel_flag && (furi_thread_flags_get() & cancel_flag);
}

static SubGhzSignalResult
    send_signal(SubGhzSignal* signal, bool use_external_radio, uint32_t cancel_flag) {
    SubGhzSignalResult result = SubGhzSignalError;
    bool enable_5v = use_external_radio && !furi_hal_power_is_otg_enabled();
    bool device_started = false;
    bool charge_suppressed = false;
    bool staged = false;
    bool started = false;
    const char* path = furi_string_get_cstr(signal->file_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    subghz_devices_init();
    if(enable_5v) subghz_radio_device_power_on();
    const SubGhzDevice* device = subghz_devices_get_by_name(
        use_external_radio ? SUBGHZ_DEVICE_CC1101_EXT_NAME : SUBGHZ_DEVICE_CC1101_INT_NAME);
    SubGhzEnvironment* environment = subghz_environment_alloc();
#ifdef APP_USES_SUBGHZ_PROTOCOL_ITEMS
    subghz_environment_set_protocol_registry(environment, (void*)&subghz_protocol_registry);
#endif
    SubGhzTransmitter* transmitter =
        subghz_transmitter_alloc_init(environment, furi_string_get_cstr(signal->protocol));
    FlipperFormat* format = flipper_format_string_alloc();
    do {
        if(subghz_signal_cancelled(cancel_flag)) {
            result = SubGhzSignalCancelled;
            break;
        }
        if(!device || !transmitter ||
           !subghz_devices_is_frequency_valid(device, signal->frequency))
            break;
        bool dynamic = subghz_transmitter_get_protocol_instance(transmitter)->protocol->type ==
                       SubGhzProtocolTypeDynamic;
        if(dynamic) {
            subghz_environment_load_keystore(environment, SUBGHZ_KEYSTORE_DIR_NAME);
            subghz_environment_load_keystore(environment, SUBGHZ_KEYSTORE_DIR_USER_NAME);
            subghz_environment_set_came_atomo_rainbow_table_file_name(
                environment, SUBGHZ_CAME_ATOMO_DIR_NAME);
            subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
                environment, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
            subghz_environment_set_nice_flor_s_rainbow_table_file_name(
                environment, SUBGHZ_NICE_FLOR_S_DIR_NAME);
        }
        __flipper_format_populate(format, signal->sub_file_contents);
        if(furi_string_equal(signal->protocol, "RAW")) {
            flipper_format_update_string_cstr(
                format, "Radio_device_name", subghz_devices_get_name(device));
        }
        if(subghz_transmitter_deserialize(transmitter, format) != SubGhzProtocolStatusOk) break;
        subghz_devices_begin(device);
        device_started = true;
        subghz_devices_reset(device);
        subghz_devices_load_preset(device, signal->preset, signal->preset_data);
        subghz_devices_set_frequency(device, signal->frequency);
        if(dynamic) {
            if(!subghz_signal_stage(format, storage, path)) {
                result = SubGhzSignalSaveError;
                break;
            }
            staged = true;
        }
        if(subghz_signal_cancelled(cancel_flag)) {
            result = SubGhzSignalCancelled;
            break;
        }
        furi_hal_power_suppress_charge_enter();
        charge_suppressed = true;
        SubGhzSignalFeed feed = {.transmitter = transmitter, .queued_until = furi_get_tick()};
        feed.deadline = feed.queued_until + furi_ms_to_ticks(5000);
        if(!subghz_devices_start_async_tx(device, subghz_signal_yield, &feed)) break;
        started = true;
        for(;;) {
            if(subghz_devices_is_async_complete_tx(device)) {
                result = SubGhzSignalComplete;
                break;
            }
            if(subghz_signal_cancelled(cancel_flag)) {
                result = SubGhzSignalCancelled;
                break;
            }
            if((int32_t)(furi_get_tick() - feed.deadline) >= 0) {
                result = SubGhzSignalTimeout;
                break;
            }
            if(cancel_flag)
                furi_thread_flags_wait(
                    cancel_flag, FuriFlagWaitAny | FuriFlagNoClear, furi_ms_to_ticks(20));
            else
                furi_delay_ms(20);
        }
        subghz_devices_stop_async_tx(device); // Stop before the stack feeder goes out of scope.
    } while(false);
    if(transmitter) {
        subghz_transmitter_stop(transmitter);
        subghz_transmitter_free(transmitter);
    }
    if(device_started) {
        subghz_devices_sleep(device);
        subghz_devices_end(device);
    }
    subghz_devices_deinit();
    if(enable_5v) subghz_radio_device_power_off();
    if(charge_suppressed) furi_hal_power_suppress_charge_exit();
    if(staged) {
        if(started) {
            // A cancelled TX may already have emitted a valid frame. Keep the advanced state.
            if(!subghz_signal_recover(storage, path)) result = SubGhzSignalSaveError;
        } else {
            FuriString* pending = furi_string_alloc_printf("%s.flip.pending", path);
            if(storage_common_remove(storage, furi_string_get_cstr(pending)) != FSE_OK)
                result = SubGhzSignalSaveError;
            furi_string_free(pending);
        }
    }
    flipper_format_free(format);
    subghz_environment_free(environment);
    furi_record_close(RECORD_STORAGE);
    return result;
}

/**
 * @brief Converts a string to a FuriHalSubGhzPreset.
 * @param preset_str The string to convert.
 * @return The FuriHalSubGhzPreset.
*/
static FuriHalSubGhzPreset signal_get_preset(FuriString* preset_str) {
    const char* preset_name = furi_string_get_cstr(preset_str);
    FuriHalSubGhzPreset preset = FuriHalSubGhzPresetIDLE;
    if(!strcmp(preset_name, "FuriHalSubGhzPresetOok270Async")) {
        preset = FuriHalSubGhzPresetOok270Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPresetOok650Async")) {
        preset = FuriHalSubGhzPresetOok650Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev238Async")) {
        preset = FuriHalSubGhzPreset2FSKDev238Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev12KAsync")) {
        preset = FuriHalSubGhzPreset2FSKDev12KAsync;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev476Async")) {
        preset = FuriHalSubGhzPreset2FSKDev476Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPresetCustom")) {
        preset = FuriHalSubGhzPresetCustom;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPresetMSK99_97KbAsync")) {
        preset = FuriHalSubGhzPresetMSK99_97KbAsync;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPresetGFSK9_99KbAsync")) {
        preset = FuriHalSubGhzPresetGFSK9_99KbAsync;
    }
    return preset;
}
#endif

/**
 * @brief Loads a SubGhzSignal from a file (supports RAW and Protocol .SUB files)
 * @param file_path The path to the file to load.
 * @return The loaded SubGhzSignal. Be sure to call subghz_signal_free.
*/
SubGhzSignal* subghz_signal_load_file(char* file_path) {
#ifdef FIRMWARE_SUPPORTS_SUBGHZ
    SubGhzSignal* signal = (SubGhzSignal*)malloc(sizeof(SubGhzSignal));
    signal->sub_file_contents = furi_string_alloc();
    signal->protocol = furi_string_alloc();
    signal->file_path = furi_string_alloc_set(file_path);
    signal->frequency = 0;
    signal->preset = FuriHalSubGhzPresetIDLE;
    signal->preset_data = NULL;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_data32;
    FileType file_type = Unknown;
    bool parsed = false;

    if(subghz_signal_recover(storage, file_path) &&
       flipper_format_file_open_existing(fff_data_file, file_path)) {
        do {
            if(!flipper_format_read_header(fff_data_file, temp_str, &temp_data32)) {
                FURI_LOG_E(TAG, "Error reading header");
                break;
            }

            if(strcmp(furi_string_get_cstr(temp_str), SUBGHZ_KEY_FILE_TYPE) == 0) {
                file_type = Key;
            } else if(strcmp(furi_string_get_cstr(temp_str), SUBGHZ_RAW_FILE_TYPE) == 0) {
                file_type = Raw;
            } else {
                FURI_LOG_E(TAG, "Unknown file type: %s", furi_string_get_cstr(temp_str));
                break;
            }

            if(!flipper_format_read_uint32(fff_data_file, "Frequency", &temp_data32, 1)) {
                FURI_LOG_E(TAG, "Missing Frequency");
                break;
            }
            signal->frequency = temp_data32;

            if(!flipper_format_read_string(fff_data_file, "Preset", temp_str)) {
                FURI_LOG_E(TAG, "Missing Preset");
                break;
            }
            signal->preset = signal_get_preset(temp_str);
            if(!signal->preset) {
                FURI_LOG_E(TAG, "Unknown preset");
                break;
            }

            if(signal->preset == FuriHalSubGhzPresetCustom) {
                uint32_t count = 0;
                if(!flipper_format_get_value_count(fff_data_file, "Custom_preset_data", &count) ||
                   count < 10 || count > 512 || (count & 1))
                    break;
                signal->preset_data = malloc(count);
                if(!signal->preset_data ||
                   !flipper_format_read_hex(
                       fff_data_file, "Custom_preset_data", signal->preset_data, count))
                    break;
                bool valid = signal->preset_data[count - 10] == 0 &&
                             signal->preset_data[count - 9] == 0;
                for(uint32_t i = 0; valid && i < count - 10; i += 2) {
                    valid = signal->preset_data[i] != 0 && signal->preset_data[i] <= 0x2E;
                }
                if(!valid) break;
            }

            if(!flipper_format_read_string(fff_data_file, "Protocol", temp_str)) {
                FURI_LOG_E(TAG, "Missing Protocol");
                break;
            }
            furi_string_set(signal->protocol, temp_str);

            parsed = true;
        } while(false);
    } else {
        FURI_LOG_E(TAG, "Error opening file %s", file_path);
    }

    flipper_format_free(fff_data_file);

    if(parsed && file_type == Raw) {
        furi_string_set(signal->sub_file_contents, temp_str);
        furi_string_printf(
            signal->sub_file_contents,
            "Filetype: RAW File Load\r\n"
            "Version: 1\r\n"
            "Protocol: RAW\r\n"
            "File_name: %s\r\n"
            // TODO: I haven't tested EXT, so not sure if there is additional setup required.
            "Radio_device_name: cc1101_int\r\n",
            file_path);
    } else if(parsed && file_type == Key) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t file_size = storage_file_size(file);
            // Protocol key files are small. RAW data is streamed by its encoder instead.
            char* file_contents = file_size && file_size <= 16384 ? malloc(file_size + 1) : NULL;
            if(file_contents && storage_file_read(file, file_contents, file_size) == file_size) {
                file_contents[file_size] = '\0';
                furi_string_set(signal->sub_file_contents, file_contents);
            } else {
                FURI_LOG_E(TAG, "Error reading file %s", file_path);
                parsed = false;
            }
            free(file_contents);
            storage_file_close(file);
        } else {
            FURI_LOG_E(TAG, "Error open file %s", file_path);
            parsed = false;
        }
        storage_file_free(file);
    }

    furi_string_free(temp_str);

    if(!parsed) {
        subghz_signal_free(signal);
        signal = NULL;
    }

    furi_record_close(RECORD_STORAGE);
    return signal;
#else
    UNUSED(file_path);
    FURI_LOG_E(TAG, "SubGhzSignal not supported");
    furi_assert(false);
    return NULL;
#endif
}

/**
 * @brief Frees a SubGhzSignal.
 * @param signal The SubGhzSignal to free.
*/
void subghz_signal_free(SubGhzSignal* signal) {
    if(signal) {
        if(signal->sub_file_contents) {
            furi_string_free(signal->sub_file_contents);
            signal->sub_file_contents = NULL;
        }
        if(signal->protocol) {
            furi_string_free(signal->protocol);
            signal->protocol = NULL;
        }
        free(signal->preset_data);
        furi_string_free(signal->file_path);
        free(signal);
    }
}

/**
 * @brief Sends a SubGhzSignal.
 * @param signal The SubGhzSignal to send.
 * @param use_external_radio Currently only the internal radio is supported (please use 'false').  Whether or not to use the external radio.
 * @param resources Resources used for sending the signal.
*/
SubGhzSignalResult subghz_signal_send(
    SubGhzSignal* signal,
    bool use_external_radio,
    Resources* resources,
    uint32_t cancel_flag) {
#ifdef FIRMWARE_SUPPORTS_SUBGHZ
    if(!signal) return SubGhzSignalError;
    uint32_t start = furi_get_tick();
    while(!resources_acquire(resources, ResourceIdSubGhzSignal, furi_ms_to_ticks(20))) {
        if(subghz_signal_cancelled(cancel_flag)) return SubGhzSignalCancelled;
        if(furi_get_tick() - start >= furi_ms_to_ticks(5000)) return SubGhzSignalTimeout;
    }
    SubGhzSignalResult result = send_signal(signal, use_external_radio, cancel_flag);
    resources_release(resources, ResourceIdSubGhzSignal);
    return result;
#else
    UNUSED(signal);
    UNUSED(use_external_radio);
    UNUSED(resources);
    UNUSED(cancel_flag);
    return SubGhzSignalError;
#endif
}
