#include "scheduler_run.h"

#include "helpers/subghz_txrx.h"

#include <furi_hal_power.h>
#include <lib/subghz/blocks/custom_btn.h>
#include <lib/subghz/blocks/generic.h>
#include <toolbox/stream/stream.h>

#define TAG "SubGhzSchedulerRun"

// Protocols are sent like a button held for this long, then they finish with their own repeats
#define SCHEDULER_TX_HOLD_MS 2000

struct ScheduleTxRun {
    Storage* storage;
    FlipperFormat* fff_head;
    FlipperFormat* fff_file;
    SubGhzTxRx* txrx;
    FuriString* protocol;
    FuriString* preset;
    FuriString* data;
    uint16_t tx_delay_ms;
    bool filetype;
};

static ScheduleTxRun* tx_run_alloc() {
    ScheduleTxRun* tx_run = calloc(1, sizeof(ScheduleTxRun));
    tx_run->storage = furi_record_open(RECORD_STORAGE);
    tx_run->fff_head = flipper_format_file_alloc(tx_run->storage);
    tx_run->fff_file = flipper_format_file_alloc(tx_run->storage);
    tx_run->txrx = subghz_txrx_alloc();
    tx_run->protocol = furi_string_alloc();
    tx_run->preset = furi_string_alloc();
    tx_run->data = furi_string_alloc();
    return tx_run;
}

static void tx_run_free(ScheduleTxRun* tx_run) {
    flipper_format_free(tx_run->fff_file);
    flipper_format_file_close(tx_run->fff_head);
    flipper_format_free(tx_run->fff_head);
    subghz_txrx_free(tx_run->txrx);
    furi_string_free(tx_run->preset);
    furi_string_free(tx_run->protocol);
    furi_string_free(tx_run->data);
    furi_record_close(RECORD_STORAGE);
    free(tx_run);
}

// Dynamic (rolling code) protocols save the new counter back to the file that was sent
static void scheduler_subghz_need_save_callback(void* context) {
    FURI_LOG_I(TAG, "Saving updated subghz signal");
    ScheduleTxRun* tx_run = context;
    const char* file_path = furi_string_get_cstr(tx_run->data);
    FlipperFormat* ff = subghz_txrx_get_fff_data(tx_run->txrx);

    Stream* ff_stream = flipper_format_get_raw_stream(ff);
    flipper_format_delete_key(ff, "Repeat");

    do {
        if(!storage_simply_remove(tx_run->storage, file_path)) {
            FURI_LOG_E(TAG, "Failed to delete subghz file before re-save");
            break;
        }
        stream_seek(ff_stream, 0, StreamOffsetFromStart);
        stream_save_to_file(ff_stream, tx_run->storage, file_path, FSOM_CREATE_ALWAYS);
        if(storage_common_stat(tx_run->storage, file_path, NULL) != FSE_OK) {
            FURI_LOG_E(TAG, "Error verifying new subghz file after re-save");
            break;
        }
    } while(0);
}

// Sleep in small steps so an abort request is picked up quickly
static void scheduler_tx_delay(SchedulerApp* app, uint32_t ms) {
    while(ms && !app->tx_abort) {
        const uint32_t step = MIN(ms, (uint32_t)10);
        furi_delay_ms(step);
        ms -= step;
    }
}

// Load the .sub file in tx_run->data into txrx fff_data, set its preset and decoder
static bool scheduler_tx_load_file(ScheduleTxRun* tx_run) {
    SubGhzTxRx* txrx = tx_run->txrx;
    const char* file_name = furi_string_get_cstr(tx_run->data);
    FlipperFormat* fff_data = subghz_txrx_get_fff_data(txrx);
    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_data32 = 0;
    uint32_t frequency = 0;
    bool ok = false;

    // Start from an empty buffer, otherwise playlist entries pile up
    stream_clean(flipper_format_get_raw_stream(fff_data));
    subghz_custom_btns_reset();

    do {
        if(!flipper_format_file_open_existing(tx_run->fff_file, file_name)) {
            FURI_LOG_E(TAG, "Error loading file: %s", file_name);
            break;
        }

        if(!flipper_format_read_header(tx_run->fff_file, temp_str, &temp_data32)) {
            FURI_LOG_E(TAG, "Missing or incorrect header");
            break;
        }
        if(((strcmp(furi_string_get_cstr(temp_str), SUBGHZ_KEY_FILE_TYPE) != 0) &&
            (strcmp(furi_string_get_cstr(temp_str), SUBGHZ_RAW_FILE_TYPE) != 0)) ||
           temp_data32 != SUBGHZ_KEY_FILE_VERSION) {
            FURI_LOG_E(TAG, "Type or version mismatch");
            break;
        }

        SubGhzSetting* setting = subghz_txrx_get_setting(txrx);
        if(!flipper_format_read_uint32(tx_run->fff_file, "Frequency", &frequency, 1)) {
            FURI_LOG_W(TAG, "Missing Frequency. Setting default frequency");
            frequency = subghz_setting_get_default_frequency(setting);
        } else if(!subghz_txrx_radio_device_is_frequecy_valid(txrx, frequency)) {
            FURI_LOG_E(TAG, "Frequency not supported on the chosen radio module");
            break;
        }

        if(!flipper_format_read_string(tx_run->fff_file, "Preset", temp_str)) {
            FURI_LOG_E(TAG, "Missing Preset");
            break;
        }
        furi_string_set_str(
            tx_run->preset, subghz_txrx_get_preset_name(txrx, furi_string_get_cstr(temp_str)));
        if(furi_string_empty(tx_run->preset)) {
            FURI_LOG_E(TAG, "Unknown preset");
            break;
        }
        if(furi_string_equal(tx_run->preset, "CUSTOM")) {
            subghz_setting_delete_custom_preset(setting, furi_string_get_cstr(tx_run->preset));
            if(!subghz_setting_load_custom_preset(
                   setting, furi_string_get_cstr(tx_run->preset), tx_run->fff_file)) {
                FURI_LOG_E(TAG, "Missing Custom preset");
                break;
            }
        }
        size_t preset_index =
            subghz_setting_get_inx_preset_by_name(setting, furi_string_get_cstr(tx_run->preset));
        subghz_txrx_set_preset(
            txrx,
            furi_string_get_cstr(tx_run->preset),
            frequency,
            subghz_setting_get_preset_data(setting, preset_index),
            subghz_setting_get_preset_data_size(setting, preset_index));

        if(!flipper_format_read_string(tx_run->fff_file, "Protocol", tx_run->protocol)) {
            FURI_LOG_E(TAG, "Missing protocol");
            break;
        }

        if(furi_string_equal(tx_run->protocol, "RAW")) {
            subghz_protocol_raw_gen_fff_data(
                fff_data, file_name, subghz_txrx_radio_device_get_name(txrx));
        } else {
            stream_copy_full(
                flipper_format_get_raw_stream(tx_run->fff_file),
                flipper_format_get_raw_stream(fff_data));
        }

        // Decoder tells txrx if the protocol is dynamic and has to be saved after TX
        if(!subghz_txrx_load_decoder_by_name_protocol(
               txrx, furi_string_get_cstr(tx_run->protocol))) {
            FURI_LOG_E(TAG, "Protocol not found: %s", furi_string_get_cstr(tx_run->protocol));
            break;
        }
        if(subghz_protocol_decoder_base_deserialize(subghz_txrx_get_decoder(txrx), fff_data) !=
           SubGhzProtocolStatusOk) {
            FURI_LOG_E(TAG, "Error parsing protocol data");
            break;
        }

        ok = true;
    } while(false);

    flipper_format_file_close(tx_run->fff_file);
    furi_string_free(temp_str);
    return ok;
}

// Send loaded signal once, like a single button press in Sub-GHz app:
// protocols TX as if the button was held for SCHEDULER_TX_HOLD_MS and then finish
// with their own repeats, RAW is sent once to the end of file. Abort stops TX right away
static void scheduler_tx_send(SchedulerApp* app, ScheduleTxRun* tx_run) {
    const bool is_raw = furi_string_equal(tx_run->protocol, "RAW");

    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    subghz_block_generic_global.endless_tx = !is_raw;
    if(subghz_txrx_tx_start(tx_run->txrx, subghz_txrx_get_fff_data(tx_run->txrx)) ==
       SubGhzTxRxStartTxStateOk) {
        const uint32_t hold_start = furi_get_tick();
        while(!app->tx_abort && !subghz_txrx_tx_is_complete(tx_run->txrx)) {
            if(subghz_block_generic_global.endless_tx &&
               furi_get_tick() - hold_start >= furi_ms_to_ticks(SCHEDULER_TX_HOLD_MS)) {
                // "Release" the button
                subghz_block_generic_global.endless_tx = false;
            }
            furi_delay_ms(5);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to start TX");
    }
    subghz_block_generic_global.endless_tx = false;

    // This will call need_save_callback, if necessary
    subghz_txrx_stop(tx_run->txrx);

    notification_message(app->notifications, &sequence_blink_stop);
}

// Send the .sub file in tx_run->data tx_count + 1 times
static bool scheduler_tx_send_file(SchedulerApp* app, ScheduleTxRun* tx_run) {
    if(!scheduler_tx_load_file(tx_run)) {
        return false;
    }

    const uint8_t tx_count = scheduler_get_tx_count(app->scheduler);
    for(uint_fast8_t i = 0; i <= tx_count && !app->tx_abort; ++i) {
        scheduler_tx_send(app, tx_run);
        scheduler_tx_delay(app, tx_run->tx_delay_ms);
    }

    // Next file may be another remote, reset rolling code state like Sub-GHz app does
    subghz_txrx_reset_dynamic_and_custom_btns(tx_run->txrx);

    return true;
}

static int32_t scheduler_tx(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;

    // Only block charging while actually transmitting, so long schedules can charge in between
    furi_hal_power_suppress_charge_enter();

    ScheduleTxRun* tx_run = tx_run_alloc();
    int32_t result = 0;

    const bool radio = scheduler_get_radio(app->scheduler);
    subghz_txrx_radio_device_set(
        tx_run->txrx, radio ? SubGhzRadioDeviceTypeExternalCC1101 : SubGhzRadioDeviceTypeInternal);
    subghz_txrx_set_need_save_callback(tx_run->txrx, scheduler_subghz_need_save_callback, tx_run);

    const int16_t line = app->tx_line;
    bool ok = true;

    tx_run->filetype = scheduler_get_file_type(app->scheduler);
    if(tx_run->filetype == SchedulerFileTypePlaylist) {
        ok = flipper_format_file_open_existing(
            tx_run->fff_head, furi_string_get_cstr(app->tx_file_path));
        // Read up to the requested line, or the first one when sending everything
        for(int16_t i = 0; ok && i <= MAX(line, 0); ++i) {
            ok = flipper_format_read_string(tx_run->fff_head, "sub", tx_run->data);
        }
        if(!ok) {
            FURI_LOG_E(TAG, "Playlist line %d not found", line);
        }
    } else {
        furi_string_set_str(tx_run->data, furi_string_get_cstr(app->tx_file_path));
    }

    tx_run->tx_delay_ms = scheduler_get_tx_delay_ms(app->scheduler);
    if(ok) {
        do {
            if(!scheduler_tx_send_file(app, tx_run)) {
                result = -1;
                break;
            }
        } while(line < 0 && tx_run->filetype == SchedulerFileTypePlaylist && !app->tx_abort &&
                flipper_format_read_string(tx_run->fff_head, "sub", tx_run->data));
    } else {
        result = -1;
    }

    subghz_txrx_sleep(tx_run->txrx);
    subghz_txrx_reset_dynamic_and_custom_btns(tx_run->txrx);
    tx_run_free(tx_run);
    furi_hal_power_suppress_charge_exit();

    return result;
}

void scheduler_start_tx(SchedulerApp* app, int16_t line) {
    if(app->thread || app->is_transmitting) {
        return;
    }
    app->tx_abort = false;
    app->tx_line = line;
    app->is_transmitting = true;
    app->thread = furi_thread_alloc_ex("SchedulerTxThread", 4096, scheduler_tx, app);
    furi_thread_start(app->thread);
}

bool scheduler_tx_finished(SchedulerApp* app) {
    return app->thread && furi_thread_get_state(app->thread) == FuriThreadStateStopped;
}

void scheduler_cleanup_tx(SchedulerApp* app) {
    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }
    app->is_transmitting = false;
}

void scheduler_stop_tx(SchedulerApp* app) {
    app->tx_abort = true;
    scheduler_cleanup_tx(app);
    app->tx_abort = false;
}
