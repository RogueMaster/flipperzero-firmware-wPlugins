#include "subghz_txrx_i.h" // IWYU pragma: keep

#include <lib/subghz/protocols/protocol_items.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/blocks/custom_btn.h>

#include <power/power_service/power.h>

#include <furi/core/memmgr.h>
#include <furi/core/memmgr_heap.h>

#define TAG "SubGhzTxRx"

// A missing-module search powers the OTG rail and runs the driver's self-test timeout.
// Keep that slow path out of reception and bound it on the idle Sub-GHz menu.
#define SUBGHZ_RADIO_DEVICE_PROBE_PERIOD_MS 5000UL

static void subghz_txrx_worker_pair_callback(void* context, bool level, uint32_t duration) {
    subghz_txrx_decode(context, level, duration);
}

static void subghz_txrx_worker_overrun_callback(void* context) {
    SubGhzTxRx* instance = context;
    subghz_receiver_reset(instance->receiver);
}

static void subghz_txrx_radio_device_power_on(SubGhzTxRx* instance) {
    Power* power = furi_record_open(RECORD_POWER);
    // A requested rail can be temporarily off while Power handles overload recovery.
    if(!power_is_otg_enabled(power) && !furi_hal_power_is_otg_enabled()) {
        power_enable_otg(power, true);
        instance->radio_device_otg_owned = true;
    }
    furi_record_close(RECORD_POWER);
}

static void subghz_txrx_radio_device_power_off(SubGhzTxRx* instance) {
    if(!instance->radio_device_otg_owned) return;
    Power* power = furi_record_open(RECORD_POWER);
    power_enable_otg(power, false);
    furi_record_close(RECORD_POWER);
    instance->radio_device_otg_owned = false;
}

SubGhzTxRx* subghz_txrx_alloc(void) {
    SubGhzTxRx* instance = malloc(sizeof(SubGhzTxRx));
    instance->transmitter = NULL;
    instance->setting = subghz_setting_alloc();
    subghz_setting_load(instance->setting, EXT_PATH("subghz/assets/setting_user.txt"));

    instance->air_time_us = 0;

    instance->preset = malloc(sizeof(SubGhzRadioPreset));
    instance->preset->name = furi_string_alloc();
    subghz_txrx_set_default_preset(instance, 0);

    instance->txrx_state = SubGhzTxRxStateSleep;

    subghz_txrx_hopper_set_state(instance, SubGhzHopperStateOFF);
    subghz_txrx_speaker_set_state(instance, SubGhzSpeakerStateDisable);
    subghz_txrx_set_debug_pin_state(instance, false);

    instance->worker = subghz_worker_alloc();
    instance->fff_data = flipper_format_string_alloc();
    instance->tx_from_internal_fff = false;

    instance->environment = subghz_environment_alloc();
    instance->is_database_loaded =
        subghz_environment_load_keystore(instance->environment, SUBGHZ_KEYSTORE_DIR_NAME);
    subghz_environment_load_keystore(instance->environment, SUBGHZ_KEYSTORE_DIR_USER_NAME);
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        instance->environment, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        instance->environment, SUBGHZ_NICE_FLOR_S_DIR_NAME);
    subghz_environment_set_protocol_registry(
        instance->environment, (void*)&subghz_protocol_registry);
    instance->receiver = subghz_receiver_alloc_init(instance->environment);
    instance->receiver_filter = 0;

    subghz_worker_set_overrun_callback(instance->worker, subghz_txrx_worker_overrun_callback);
    subghz_worker_set_pair_callback(instance->worker, subghz_txrx_worker_pair_callback);
    subghz_worker_set_context(instance->worker, instance);

    instance->radio_device_type = SubGhzRadioDeviceTypeInternal;
    instance->radio_device_external_wanted = false;
    instance->radio_device_probe_tick = furi_get_tick();
    instance->radio_device_otg_owned = false;

#ifndef SUBGHZ_ADD_MANUALLY
    //set default device External
    subghz_devices_init();
    instance->radio_device_type =
        subghz_txrx_radio_device_set(instance, SubGhzRadioDeviceTypeExternalCC1101);
#endif

    return instance;
}

void subghz_txrx_free(SubGhzTxRx* instance) {
    furi_assert(instance);

#ifndef SUBGHZ_ADD_MANUALLY
    if(instance->radio_device_type != SubGhzRadioDeviceTypeInternal) {
        subghz_devices_end(instance->radio_device);
    }
    subghz_txrx_radio_device_power_off(instance);

    subghz_devices_deinit();
#endif

    subghz_worker_free(instance->worker);
    subghz_receiver_free(instance->receiver);
    subghz_environment_free(instance->environment);
    flipper_format_free(instance->fff_data);
    furi_string_free(instance->preset->name);
    subghz_setting_free(instance->setting);

    free(instance->preset);
    free(instance);
}

bool subghz_txrx_is_database_loaded(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->is_database_loaded;
}

void subghz_txrx_set_preset(
    SubGhzTxRx* instance,
    const char* preset_name,
    uint32_t frequency,
    float latitude,
    float longitude,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_assert(instance);
    furi_string_set(instance->preset->name, preset_name);

    SubGhzRadioPreset* preset = instance->preset;
    preset->frequency = frequency;
    preset->latitude = latitude;
    preset->longitude = longitude;
    preset->data = preset_data;
    preset->data_size = preset_data_size;
}

uint8_t*
    subghz_txrx_set_tx_power(uint8_t* preset_data, size_t preset_data_size, uint8_t tx_power) {
#define PRESET_POWER_OFFSET_FM 8
#define PRESET_POWER_OFFSET_AM 7
#define TX_PATABLE_OFFSET_AM   8
#define TX_PATABLE_COUNT       17

    //I had to skip the +10dBM and -6dBm Values, use only ones AM/FM have in common.
    //Highest Value is 12dBm for AM, 10 for FM. So Menu needs to reflect that.
    static const uint8_t tx_pa_table[TX_PATABLE_COUNT] = {
        0,
        0xC0, //12dBm
        0xCD, //7dBm
        0x86, //5dBm
        0x50, //0dBm
        0x26, // -10dBm
        0x1D, // -15dBm
        0x17, //-20dBm
        0x03, //-30dBm
        0xC0, // 10dBm
        0xC8, //7dBm
        0x84, //5dBm
        0x60, //0dBm
        0x34, //-10dBm
        0x1D, //-15dBm
        0x0E, // -20dBm
        0x12, //-30dBm
    };

    //Grab the AM and FM byte now, so we can do proper checks.
    uint8_t fm_byte = preset_data[preset_data_size - PRESET_POWER_OFFSET_FM];
    uint8_t am_byte = preset_data[preset_data_size - PRESET_POWER_OFFSET_AM];

    //Set the TX Power Here in the CC1101 register...

    //If we have both bytes 1st bytes set or none, this isnt a preset we can deal with here.
    if(fm_byte && !am_byte) {
        //Use FM Table
        if(tx_power) {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_FM] =
                tx_pa_table[TX_PATABLE_OFFSET_AM + tx_power];
        } else {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_FM] =
                tx_pa_table[1]; //Max Power 0xC0 10dBm
        }
    } else if(am_byte && !fm_byte) {
        //Use AM Table
        if(tx_power) {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_AM] = tx_pa_table[tx_power];
        } else {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_AM] =
                tx_pa_table[1]; //Max Power 0xC0 12dBm
        }
    }

    //Pass back the preset_so we can call one liners.
    return preset_data;
}

const char* subghz_txrx_get_preset_name(SubGhzTxRx* instance, const char* preset) {
    UNUSED(instance);
    const char* preset_name = "";
    if(!strcmp(preset, "FuriHalSubGhzPresetOok270Async")) {
        preset_name = "AM270";
    } else if(!strcmp(preset, "FuriHalSubGhzPresetOok650Async")) {
        preset_name = "AM650";
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev238Async")) {
        preset_name = "FM238";
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev12KAsync")) {
        preset_name = "FM12K";
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev476Async")) {
        preset_name = "FM476";
    } else if(!strcmp(preset, "FuriHalSubGhzPresetCustom")) {
        preset_name = "CUSTOM";
    } else {
        FURI_LOG_E(TAG, "Unknown preset");
    }
    return preset_name;
}

SubGhzRadioPreset subghz_txrx_get_preset(SubGhzTxRx* instance) {
    furi_assert(instance);
    return *instance->preset;
}

void subghz_txrx_get_frequency_and_modulation(
    SubGhzTxRx* instance,
    FuriString* frequency,
    FuriString* modulation,
    bool long_name) {
    furi_assert(instance);
    SubGhzRadioPreset* preset = instance->preset;
    if(frequency != NULL) {
        furi_string_printf(
            frequency,
            "%03ld.%02ld",
            preset->frequency / 1000000 % 1000,
            preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        if(long_name) {
            furi_string_printf(modulation, "%s", furi_string_get_cstr(preset->name));
        } else {
            furi_string_printf(modulation, "%.2s", furi_string_get_cstr(preset->name));
        }
    }
}

float subghz_txrx_get_latitude(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->preset->latitude;
}

float subghz_txrx_get_longitude(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->preset->longitude;
}

static bool subghz_txrx_radio_device_probe(SubGhzTxRx* instance) {
    // Stamp every probe, including loss detection, so the next menu tick does not
    // immediately search again for a module that just disappeared.
    instance->radio_device_probe_tick = furi_get_tick();
    const SubGhzRadioDeviceType previous = instance->radio_device_type;
    if(subghz_txrx_radio_device_set(instance, SubGhzRadioDeviceTypeExternalCC1101) == previous) {
        return false;
    }
    FURI_LOG_I(TAG, "Radio device is now %s", subghz_txrx_radio_device_get_name(instance));
    return true;
}

static bool subghz_txrx_radio_device_poll_possible(SubGhzTxRx* instance) {
    if(instance->txrx_state != SubGhzTxRxStateIDLE &&
       instance->txrx_state != SubGhzTxRxStateSleep) {
        return false;
    }
    return instance->radio_device_external_wanted;
}

bool subghz_txrx_radio_device_poll(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(!subghz_txrx_radio_device_poll_possible(instance)) return false;
    if(instance->radio_device_type != SubGhzRadioDeviceTypeExternalCC1101) return false;
    // An initialized module costs one status read; a missing one falls back via set().
    if(subghz_devices_is_connect(instance->radio_device)) return false;
    return subghz_txrx_radio_device_probe(instance);
}

bool subghz_txrx_radio_device_poll_reacquire(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(!subghz_txrx_radio_device_poll_possible(instance)) return false;
    if(instance->radio_device_type == SubGhzRadioDeviceTypeExternalCC1101) {
        return subghz_txrx_radio_device_poll(instance);
    }
    if(furi_get_tick() - instance->radio_device_probe_tick <
       furi_ms_to_ticks(SUBGHZ_RADIO_DEVICE_PROBE_PERIOD_MS)) {
        return false;
    }
    return subghz_txrx_radio_device_probe(instance);
}

bool subghz_txrx_radio_device_poll_active(SubGhzTxRx* instance) {
    furi_assert(instance);
    // Never move a running transmitter or second-guess an explicit Internal choice.
    if(instance->txrx_state == SubGhzTxRxStateTx || !instance->radio_device_external_wanted) {
        return false;
    }
    if(instance->radio_device_type != SubGhzRadioDeviceTypeExternalCC1101) return false;
    if(subghz_devices_is_connect(instance->radio_device)) return false;

    const bool was_rx = instance->txrx_state == SubGhzTxRxStateRx;
    // The worker must be joined and async capture stopped before its device is freed.
    subghz_txrx_stop(instance);
    const bool changed = subghz_txrx_radio_device_poll(instance);
    if(was_rx) {
        // RAW reset discards its pending capture buffer. Keep those samples while
        // restarting the radio; decoded reception can discard partial frames.
        if(!(instance->receiver_filter & SubGhzProtocolFlag_RAW)) {
            subghz_receiver_reset(instance->receiver);
        }
        subghz_txrx_rx_start(instance);
    }
    return changed;
}

static void subghz_txrx_begin(SubGhzTxRx* instance, uint8_t* preset_data) {
    furi_assert(instance);
    subghz_devices_reset(instance->radio_device);
    subghz_devices_idle(instance->radio_device);
    subghz_devices_load_preset(instance->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    instance->txrx_state = SubGhzTxRxStateIDLE;
}

static uint32_t subghz_txrx_rx(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    furi_assert(
        instance->txrx_state != SubGhzTxRxStateRx && instance->txrx_state != SubGhzTxRxStateSleep);

    subghz_devices_idle(instance->radio_device);

    uint32_t value = subghz_devices_set_frequency(instance->radio_device, frequency);
    subghz_devices_flush_rx(instance->radio_device);
    subghz_txrx_speaker_on(instance);

    subghz_devices_start_async_rx(
        instance->radio_device, subghz_worker_rx_callback, instance->worker);
    subghz_worker_start(instance->worker);
    instance->txrx_state = SubGhzTxRxStateRx;
    return value;
}

static void subghz_txrx_idle(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->txrx_state != SubGhzTxRxStateSleep) {
        subghz_devices_idle(instance->radio_device);
        subghz_txrx_speaker_off(instance);
        instance->txrx_state = SubGhzTxRxStateIDLE;
    }
}

static void subghz_txrx_rx_end(SubGhzTxRx* instance) {
    furi_assert(instance);
    furi_assert(instance->txrx_state == SubGhzTxRxStateRx);

    if(subghz_worker_is_running(instance->worker)) {
        subghz_worker_stop(instance->worker);
        subghz_devices_stop_async_rx(instance->radio_device);
    }
    subghz_devices_idle(instance->radio_device);
    subghz_txrx_speaker_off(instance);
    instance->txrx_state = SubGhzTxRxStateIDLE;
}

void subghz_txrx_sleep(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_devices_sleep(instance->radio_device);
    instance->txrx_state = SubGhzTxRxStateSleep;
}

static bool subghz_txrx_tx(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    furi_assert(instance->txrx_state != SubGhzTxRxStateSleep);

    subghz_devices_idle(instance->radio_device);
    subghz_devices_set_frequency(instance->radio_device, frequency);

    bool ret = subghz_devices_set_tx(instance->radio_device);
    if(ret) {
        subghz_txrx_speaker_on(instance);
        instance->txrx_state = SubGhzTxRxStateTx;
    }

    return ret;
}

SubGhzTxRxStartTxState subghz_txrx_tx_start(SubGhzTxRx* instance, FlipperFormat* flipper_format) {
    furi_assert(instance);
    furi_assert(flipper_format);

    subghz_txrx_stop(instance);
    // RAW deserialize starts a file worker bound to a device. Select before that
    // worker exists; begin() later must not swap a device out from under it.
    subghz_txrx_radio_device_poll(instance);

    //Only a transmission of our own fff_data corresponds to subghz->file_path
    //and may be saved back after TX. History/RX signals (passed as a separate
    //flipper_format) must never trigger the save-back
    instance->tx_from_internal_fff = (flipper_format == instance->fff_data);

    SubGhzTxRxStartTxState ret = SubGhzTxRxStartTxStateErrorParserOthers;
    FuriString* temp_str = furi_string_alloc();
    do {
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            break;
        }

        size_t need_heap = SUBGHZ_TX_MIN_HEAP;
        size_t need_block = SUBGHZ_TX_MIN_BLOCK;
        if(furi_string_equal(temp_str, "RAW")) {
            need_heap = SUBGHZ_TX_MIN_HEAP_RAW;
            need_block = SUBGHZ_TX_MIN_BLOCK_RAW;
            if(!flipper_format_rewind(flipper_format) ||
               !flipper_format_update_string_cstr(
                   flipper_format,
                   "Radio_device_name",
                   subghz_txrx_radio_device_get_name(instance))) {
                FURI_LOG_E(TAG, "Unable to update RAW radio device");
                break;
            }
        }
        instance->tx_min_heap_required = need_heap;
        if(memmgr_get_free_heap() < need_heap || memmgr_heap_get_max_free_block() < need_block) {
            FURI_LOG_E(TAG, "Not enough memory to start TX");
            ret = SubGhzTxRxStartTxStateErrorMemory;
            break;
        }

        ret = SubGhzTxRxStartTxStateOk;

        SubGhzRadioPreset* preset = instance->preset;
        instance->transmitter =
            subghz_transmitter_alloc_init(instance->environment, furi_string_get_cstr(temp_str));

        if(instance->transmitter) {
            if(subghz_transmitter_deserialize(instance->transmitter, flipper_format) ==
               SubGhzProtocolStatusOk) {
                if(strcmp(furi_string_get_cstr(preset->name), "") != 0) {
                    subghz_txrx_begin(
                        instance,
                        subghz_setting_get_preset_data_by_name(
                            instance->setting, furi_string_get_cstr(preset->name)));
                    if(preset->frequency) {
                        if(!subghz_txrx_tx(instance, preset->frequency)) {
                            FURI_LOG_E(TAG, "Only Rx");
                            ret = SubGhzTxRxStartTxStateErrorOnlyRx;
                        }
                    } else {
                        ret = SubGhzTxRxStartTxStateErrorParserOthers;
                    }

                } else {
                    FURI_LOG_E(
                        TAG, "Unknown name preset \" %s \"", furi_string_get_cstr(preset->name));
                    ret = SubGhzTxRxStartTxStateErrorParserOthers;
                }

                if(ret == SubGhzTxRxStartTxStateOk) {
                    //Start TX
                    if(!subghz_devices_start_async_tx(
                           instance->radio_device,
                           subghz_transmitter_yield,
                           instance->transmitter)) {
                        // The driver has already unwound a failed async start.
                        ret = SubGhzTxRxStartTxStateErrorOnlyRx;
                    }
                }
            } else {
                ret = SubGhzTxRxStartTxStateErrorParserOthers;
            }
        } else {
            FURI_LOG_E(TAG, "Protocol \"%s\" has no encoder", furi_string_get_cstr(temp_str));
            ret = SubGhzTxRxStartTxStateErrorParserOthers;
        }
        if(ret != SubGhzTxRxStartTxStateOk) {
            if(instance->transmitter) {
                subghz_transmitter_free(instance->transmitter);
                instance->transmitter = NULL;
            }
            if(instance->txrx_state != SubGhzTxRxStateIDLE) {
                subghz_txrx_idle(instance);
            }
        }

    } while(false);
    furi_string_free(temp_str);
    return ret;
}

void subghz_txrx_rx_start(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_stop(instance);
    subghz_txrx_radio_device_poll(instance);
    subghz_txrx_begin(
        instance,
        subghz_setting_get_preset_data_by_name(
            subghz_txrx_get_setting(instance), furi_string_get_cstr(instance->preset->name)));
    subghz_txrx_rx(instance, instance->preset->frequency);
}

void subghz_txrx_set_need_save_callback(
    SubGhzTxRx* instance,
    SubGhzTxRxNeedSaveCallback callback,
    void* context) {
    furi_assert(instance);
    instance->need_save_callback = callback;
    instance->need_save_context = context;
}

static void subghz_txrx_tx_stop(SubGhzTxRx* instance) {
    furi_assert(instance);
    furi_assert(instance->txrx_state == SubGhzTxRxStateTx);
    //Stop TX
    subghz_devices_stop_async_tx(instance->radio_device);
    subghz_transmitter_stop(instance->transmitter);
    subghz_transmitter_free(instance->transmitter);
    instance->transmitter = NULL;

    //if protocol dynamic then we save the last upload
    //but only when we transmitted our own fff_data (bound to file_path)
    //never for history/RX signals, which would overwrite an unrelated file
    if(instance->tx_from_internal_fff &&
       instance->decoder_result->protocol->type == SubGhzProtocolTypeDynamic) {
        if(instance->need_save_callback) {
            instance->need_save_callback(instance->need_save_context);
        }
    }
    subghz_txrx_idle(instance);
    subghz_txrx_speaker_off(instance);
}

FlipperFormat* subghz_txrx_get_fff_data(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->fff_data;
}

size_t subghz_txrx_get_tx_min_heap_required(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->tx_min_heap_required;
}

SubGhzSetting* subghz_txrx_get_setting(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->setting;
}

void subghz_txrx_stop(SubGhzTxRx* instance) {
    furi_assert(instance);

    switch(instance->txrx_state) {
    case SubGhzTxRxStateTx:
        subghz_txrx_tx_stop(instance);
        subghz_txrx_speaker_unmute(instance);
        break;
    case SubGhzTxRxStateRx:
        subghz_txrx_rx_end(instance);
        subghz_txrx_speaker_mute(instance);
        break;

    default:
        break;
    }
}

void subghz_txrx_hopper_update(SubGhzTxRx* instance, float stay_threshold) {
    furi_assert(instance);

    switch(instance->hopper_state) {
    case SubGhzHopperStateOFF:
    case SubGhzHopperStatePause:
        return;
    case SubGhzHopperStateRSSITimeOut:
        if(instance->hopper_timeout != 0) {
            instance->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    //    Init value isn't using
    //    float rssi = -127.0f;
    if(instance->hopper_state != SubGhzHopperStateRSSITimeOut) {
        // See RSSI Calculation timings in CC1101 17.3 RSSI
        float rssi = subghz_devices_get_rssi(instance->radio_device);

        // Stay if RSSI is high enough
        if(rssi > stay_threshold) {
            instance->hopper_timeout = 10;
            instance->hopper_state = SubGhzHopperStateRSSITimeOut;
            return;
        }
    } else {
        instance->hopper_state = SubGhzHopperStateRunning;
    }
    // Select next frequency
    if(instance->hopper_idx_frequency <
       subghz_setting_get_hopper_frequency_count(instance->setting) - 1) {
        instance->hopper_idx_frequency++;
    } else {
        instance->hopper_idx_frequency = 0;
    }

    if(instance->txrx_state == SubGhzTxRxStateRx) {
        subghz_txrx_rx_end(instance);
    }
    if(instance->txrx_state == SubGhzTxRxStateIDLE) {
        // Callers run poll_active() earlier in the tick; do not re-probe per hop.
        subghz_receiver_reset(instance->receiver);
        instance->preset->frequency =
            subghz_setting_get_hopper_frequency(instance->setting, instance->hopper_idx_frequency);
        subghz_txrx_rx(instance, instance->preset->frequency);
    }
}

SubGhzHopperState subghz_txrx_hopper_get_state(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->hopper_state;
}

void subghz_txrx_hopper_set_state(SubGhzTxRx* instance, SubGhzHopperState state) {
    furi_assert(instance);
    instance->hopper_state = state;
}

void subghz_txrx_hopper_unpause(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->hopper_state == SubGhzHopperStatePause) {
        instance->hopper_state = SubGhzHopperStateRunning;
    }
}

void subghz_txrx_hopper_pause(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->hopper_state == SubGhzHopperStateRunning) {
        instance->hopper_state = SubGhzHopperStatePause;
    }
}

void subghz_txrx_speaker_on(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_ibutton);
    }

    if(instance->speaker_state == SubGhzSpeakerStateEnable) {
        if(furi_hal_speaker_acquire(30)) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_speaker);
            }
        } else {
            instance->speaker_state = SubGhzSpeakerStateDisable;
        }
    }
}

void subghz_txrx_speaker_off(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
    }
    if(instance->speaker_state != SubGhzSpeakerStateDisable) {
        if(furi_hal_speaker_is_mine()) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
            }
            furi_hal_speaker_release();
            if(instance->speaker_state == SubGhzSpeakerStateShutdown)
                instance->speaker_state = SubGhzSpeakerStateDisable;
        }
    }
}

void subghz_txrx_speaker_mute(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
    }
    if(instance->speaker_state == SubGhzSpeakerStateEnable) {
        if(furi_hal_speaker_is_mine()) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
            }
        }
    }
}

void subghz_txrx_speaker_unmute(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_ibutton);
    }
    if(instance->speaker_state == SubGhzSpeakerStateEnable) {
        if(furi_hal_speaker_is_mine()) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_speaker);
            }
        }
    }
}

void subghz_txrx_speaker_set_state(SubGhzTxRx* instance, SubGhzSpeakerState state) {
    furi_assert(instance);
    instance->speaker_state = state;
}

SubGhzSpeakerState subghz_txrx_speaker_get_state(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->speaker_state;
}

bool subghz_txrx_load_decoder_by_name_protocol(SubGhzTxRx* instance, const char* name_protocol) {
    furi_assert(instance);
    furi_assert(name_protocol);
    bool res = false;
    instance->decoder_result =
        subghz_receiver_search_decoder_base_by_name(instance->receiver, name_protocol);
    if(instance->decoder_result) {
        res = true;
    }
    return res;
}

SubGhzProtocolDecoderBase* subghz_txrx_get_decoder(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->decoder_result;
}

bool subghz_txrx_protocol_is_serializable(SubGhzTxRx* instance) {
    furi_assert(instance);
    return (instance->decoder_result->protocol->flag & SubGhzProtocolFlag_Save) ==
           SubGhzProtocolFlag_Save;
}

bool subghz_txrx_protocol_is_transmittable(SubGhzTxRx* instance, bool check_type) {
    furi_assert(instance);
    const SubGhzProtocol* protocol = instance->decoder_result->protocol;
    if(check_type) {
        return ((protocol->flag & SubGhzProtocolFlag_Send) == SubGhzProtocolFlag_Send) &&
               protocol->encoder->deserialize && protocol->type == SubGhzProtocolTypeStatic;
    }
    return ((protocol->flag & SubGhzProtocolFlag_Send) == SubGhzProtocolFlag_Send) &&
           protocol->encoder->deserialize;
}

void subghz_txrx_receiver_set_filter(SubGhzTxRx* instance, SubGhzProtocolFlag filter) {
    furi_assert(instance);
    instance->receiver_filter = filter;
    subghz_receiver_set_filter(instance->receiver, filter);
}

void subghz_txrx_receiver_set_ignore_filter(
    SubGhzTxRx* instance,
    SubGhzProtocolFilter ignore_filter) {
    furi_assert(instance);
    subghz_receiver_set_ignore_filter(instance->receiver, ignore_filter);
}

void subghz_txrx_set_rx_callback(
    SubGhzTxRx* instance,
    SubGhzReceiverCallback callback,
    void* context) {
    subghz_receiver_set_rx_callback(instance->receiver, callback, context);
}

void subghz_txrx_set_raw_file_encoder_worker_callback_end(
    SubGhzTxRx* instance,
    SubGhzProtocolEncoderRAWCallbackEnd callback,
    void* context) {
    subghz_protocol_raw_file_encoder_worker_set_callback_end(
        (SubGhzProtocolEncoderRAW*)subghz_transmitter_get_protocol_instance(instance->transmitter),
        callback,
        context);
}

bool subghz_txrx_radio_device_is_external_connected(SubGhzTxRx* instance, const char* name) {
    furi_assert(instance);

    bool is_connect = false;
    bool was_otg_owned = instance->radio_device_otg_owned;
    subghz_txrx_radio_device_power_on(instance);

    const SubGhzDevice* device = subghz_devices_get_by_name(name);
    if(device) {
        is_connect = subghz_devices_is_connect(device);
    }

    if(!was_otg_owned) {
        subghz_txrx_radio_device_power_off(instance);
    }
    return is_connect;
}

SubGhzRadioDeviceType
    subghz_txrx_radio_device_set(SubGhzTxRx* instance, SubGhzRadioDeviceType radio_device_type) {
    furi_assert(instance);

    if(instance->txrx_state != SubGhzTxRxStateIDLE &&
       instance->txrx_state != SubGhzTxRxStateSleep) {
        FURI_LOG_W(TAG, "Stop the radio before selecting another device");
        return instance->radio_device_type;
    }

    instance->radio_device_external_wanted = radio_device_type ==
                                             SubGhzRadioDeviceTypeExternalCC1101;

    // begin() performs the actual module self-test, so release any earlier instance first.
    if(instance->radio_device_type != SubGhzRadioDeviceTypeInternal) {
        subghz_devices_end(instance->radio_device);
    }

    if(radio_device_type == SubGhzRadioDeviceTypeExternalCC1101) {
        instance->radio_device_probe_tick = furi_get_tick();
        subghz_txrx_radio_device_power_on(instance);
        const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        if(device == NULL) {
            FURI_LOG_E(TAG, "No %s driver loaded", SUBGHZ_DEVICE_CC1101_EXT_NAME);
        } else if(subghz_devices_begin(device)) {
            instance->radio_device = device;
            instance->radio_device_type = SubGhzRadioDeviceTypeExternalCC1101;
            return instance->radio_device_type;
        } else {
            // A failed self-test still allocated driver state that must be released.
            subghz_devices_end(device);
            FURI_LOG_W(TAG, "External radio did not answer, using internal");
        }
    }

    subghz_txrx_radio_device_power_off(instance);
    instance->radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    instance->radio_device_type = SubGhzRadioDeviceTypeInternal;

    return instance->radio_device_type;
}

SubGhzRadioDeviceType subghz_txrx_radio_device_get(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->radio_device_type;
}

float subghz_txrx_radio_device_get_rssi(SubGhzTxRx* instance) {
    furi_assert(instance);
    return subghz_devices_get_rssi(instance->radio_device);
}

const char* subghz_txrx_radio_device_get_name(SubGhzTxRx* instance) {
    furi_assert(instance);
    return subghz_devices_get_name(instance->radio_device);
}

bool subghz_txrx_radio_device_is_frequency_valid(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    return subghz_devices_is_frequency_valid(instance->radio_device, frequency);
}

SubGhzTx subghz_txrx_radio_device_check_tx(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    return subghz_devices_check_tx(instance->radio_device, frequency);
}

void subghz_txrx_set_debug_pin_state(SubGhzTxRx* instance, bool state) {
    furi_assert(instance);
    instance->debug_pin_state = state;
}

bool subghz_txrx_get_debug_pin_state(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->debug_pin_state;
}

void subghz_txrx_reset_dynamic_and_custom_btns(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_environment_reset_keeloq(instance->environment);

    faac_slh_reset_prog_mode();

    subghz_custom_btns_reset();
}

SubGhzReceiver* subghz_txrx_get_receiver(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->receiver;
}

//Every sample reaches the decoders through here so that the app can keep its
//own clock of how much air has been decoded. The sample is fed first and
//counted afterwards: a decoder reports a frame from inside
//subghz_receiver_decode(), and the duration being fed at that moment is the gap
//that terminated the frame, so during the callback the clock reads the end of
//the frame's data rather than the end of the silence that followed it
void subghz_txrx_decode(SubGhzTxRx* instance, bool level, uint32_t duration) {
    furi_assert(instance);
    subghz_receiver_decode(instance->receiver, level, duration);
    instance->air_time_us += duration;
}

uint32_t subghz_txrx_get_air_time_ms(SubGhzTxRx* instance) {
    furi_assert(instance);
    return (uint32_t)(instance->air_time_us / 1000);
}

void subghz_txrx_set_default_preset(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);

    const char* default_modulation = "AM650";
    if(frequency == 0) {
        frequency = subghz_setting_get_default_frequency(subghz_txrx_get_setting(instance));
    }
    subghz_txrx_set_preset(instance, default_modulation, frequency, NAN, NAN, NULL, 0);
}

const char* subghz_txrx_set_preset_internal(
    SubGhzTxRx* instance,
    uint32_t frequency,
    uint8_t index,
    uint8_t tx_power) {
    furi_assert(instance);

    //Grab the prset name.
    SubGhzSetting* setting = subghz_txrx_get_setting(instance);
    const char* preset_name = subghz_setting_get_preset_name(setting, index);
    subghz_setting_set_default_frequency(setting, frequency);

    //Get the preset data now so we can set TX power.
    uint8_t* preset_data = subghz_setting_get_preset_data(setting, index);
    size_t preset_data_size = subghz_setting_get_preset_data_size(setting, index);

    //Edit TX power, if necessary.
    subghz_txrx_set_tx_power(preset_data, preset_data_size, tx_power);

    //Set the Updated Preset.
    subghz_txrx_set_preset(
        instance, preset_name, frequency, NAN, NAN, preset_data, preset_data_size);

    return preset_name;
}
