#include "rfs_tx.h"
#include "../protocol/rfs_protocol.h"

#include <flipper_format/flipper_format.h>
#include <furi_hal_gpio.h>
#include <furi_hal_region.h>
#include <furi_hal_subghz.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/raw.h>
#include <storage/storage.h>

/* Upper bound for one transmission; long raw recordings stream well within this. */
#define RFS_TX_TIMEOUT_MS (60 * 1000)
#define RFS_TX_PATH_LEN   128

struct RfsTx {
    RfsRadio* radio;
    RfsDiag* diag;
    FuriThread* thread;
    FuriMutex* mutex;

    /* Request */
    char path[RFS_TX_PATH_LEN];
    RfsRadioMode mode;
    RfsTxDoneCallback callback;
    void* context;
    volatile bool cancel;

    /* Result */
    RfsTxState state;
    RfsTxError error;
    RfsRadioResult radio_result;
    RfsTxPermission permission;
    RfsDeviceId device_id;
    bool running;
};

static void rfs_tx_set_state(RfsTx* tx, RfsTxState state) {
    furi_mutex_acquire(tx->mutex, FuriWaitForever);
    tx->state = state;
    furi_mutex_release(tx->mutex);
}

static RfsTxError rfs_tx_fail(RfsTx* tx, RfsTxError error) {
    tx->error = error;
    return error;
}

/* Load the preset registers named in the file into out (static table or custom data). */
static bool rfs_tx_read_preset(
    RfsTx* tx,
    FlipperFormat* ff,
    FuriString* tmp,
    uint8_t** custom,
    const uint8_t** data,
    size_t* size) {
    flipper_format_rewind(ff);
    if(!flipper_format_read_string(ff, "Preset", tmp)) return false;
    char name[RFS_PRESET_LEN];
    if(rfs_radio_preset_from_file(furi_string_get_cstr(tmp), name, sizeof(name))) {
        return rfs_radio_preset_data(tx->radio, name, data, size);
    }
    /* Custom preset: registers are stored in the file itself. */
    uint32_t count = 0;
    flipper_format_rewind(ff);
    if(!flipper_format_get_value_count(ff, "Custom_preset_data", &count) || count == 0 ||
       count > 128)
        return false;
    *custom = malloc(count);
    flipper_format_rewind(ff);
    if(!flipper_format_read_hex(ff, "Custom_preset_data", *custom, count)) return false;
    *data = *custom;
    *size = count;
    return true;
}

static RfsTxError rfs_tx_run(RfsTx* tx) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    SubGhzEnvironment* env = NULL;
    SubGhzTransmitter* transmitter = NULL;
    FlipperFormat* raw_ff = NULL;
    uint8_t* custom_preset = NULL;
    bool radio_acquired = false;
    RfsTxError result = RfsTxErrNone;
    const SubGhzDevice* device = NULL;

    do {
        /* Validating (TX-001, TX-002) */
        rfs_tx_set_state(tx, RfsTxStateValidating);
        if(!storage_file_exists(storage, tx->path)) {
            result = rfs_tx_fail(tx, RfsTxErrFileMissing);
            break;
        }
        uint32_t version = 0;
        uint32_t frequency = 0;
        if(!flipper_format_file_open_existing(ff, tx->path) ||
           !flipper_format_read_header(ff, tmp, &version) ||
           !flipper_format_read_uint32(ff, "Frequency", &frequency, 1)) {
            result = rfs_tx_fail(tx, RfsTxErrFileInvalid);
            break;
        }
        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "Protocol", tmp)) {
            result = rfs_tx_fail(tx, RfsTxErrFileInvalid);
            break;
        }
        char protocol_name[RFS_PROTOCOL_LEN];
        strlcpy(protocol_name, furi_string_get_cstr(tmp), sizeof(protocol_name));
        bool is_raw = strcmp(protocol_name, SUBGHZ_PROTOCOL_RAW_NAME) == 0;

        const SubGhzProtocol* protocol = rfs_protocol_find(protocol_name);
        if(!protocol) {
            result = rfs_tx_fail(tx, RfsTxErrProtocolUnknown);
            break;
        }
        if(rfs_protocol_kind(protocol) == RfsKindDynamic) {
            /* Rolling codes are never replayed as static commands (Section 10.3). */
            result = rfs_tx_fail(tx, RfsTxErrRollingCode);
            break;
        }
        if(!rfs_protocol_can_send(protocol)) {
            result = rfs_tx_fail(tx, RfsTxErrNotSendable);
            break;
        }

        const uint8_t* preset_data = NULL;
        size_t preset_size = 0;
        if(!rfs_tx_read_preset(tx, ff, tmp, &custom_preset, &preset_data, &preset_size)) {
            result = rfs_tx_fail(tx, RfsTxErrPreset);
            break;
        }

        /* Resolve radio and verify frequency and permission (TX-003 to TX-006). */
        tx->radio_result = rfs_radio_resolve(
            tx->radio, tx->mode, frequency, true, &tx->device_id, &tx->permission);
        if(tx->radio_result != RfsRadioOk) {
            result = rfs_tx_fail(tx, RfsTxErrRadio);
            break;
        }
        if(tx->cancel) break;

        /* Preparing */
        rfs_tx_set_state(tx, RfsTxStatePreparing);
        env = rfs_protocol_env_alloc();
        transmitter = subghz_transmitter_alloc_init(env, protocol_name);
        if(!transmitter) {
            result = rfs_tx_fail(tx, RfsTxErrEncoder);
            break;
        }
        SubGhzProtocolStatus status;
        if(is_raw) {
            /* The RAW encoder streams the file itself, so large recordings stay on the card. */
            raw_ff = flipper_format_string_alloc();
            device = rfs_radio_device(tx->radio, tx->device_id);
            subghz_protocol_raw_gen_fff_data(raw_ff, tx->path, subghz_devices_get_name(device));
            flipper_format_rewind(raw_ff);
            status = subghz_transmitter_deserialize(transmitter, raw_ff);
        } else {
            flipper_format_rewind(ff);
            status = subghz_transmitter_deserialize(transmitter, ff);
        }
        if(status != SubGhzProtocolStatusOk) {
            result = rfs_tx_fail(tx, RfsTxErrEncoder);
            break;
        }

        /* Never key up outside the region's allowed bands (RF-011). */
        if(!furi_hal_region_is_frequency_allowed(frequency) ||
           !furi_hal_subghz_is_frequency_valid(frequency)) {
            tx->radio_result = RfsRadioErrTxBlocked;
            tx->permission = RfsTxPermBlockedRegion;
            result = rfs_tx_fail(tx, RfsTxErrRadio);
            break;
        }

        tx->radio_result = rfs_radio_acquire(tx->radio, tx->device_id);
        if(tx->radio_result != RfsRadioOk) {
            result = rfs_tx_fail(tx, RfsTxErrRadio);
            break;
        }
        radio_acquired = true;
        device = rfs_radio_device(tx->radio, tx->device_id);

        subghz_devices_load_preset(device, FuriHalSubGhzPresetCustom, (uint8_t*)preset_data);
        subghz_devices_set_frequency(device, frequency);
        const GpioPin* data_pin = subghz_devices_get_data_gpio(device);
        furi_hal_gpio_init(data_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(data_pin, false);
        if(!subghz_devices_set_tx(device)) {
            /* The firmware refused: respect it (RF-011). */
            tx->radio_result = RfsRadioErrTxBlocked;
            tx->permission = rfs_firmware_check_tx(device, frequency);
            result = rfs_tx_fail(tx, RfsTxErrRadio);
            break;
        }
        if(tx->cancel) break;

        /* Transmitting (TX-008) */
        rfs_tx_set_state(tx, RfsTxStateTransmitting);
        if(!subghz_devices_start_async_tx(device, subghz_transmitter_yield, transmitter)) {
            result = rfs_tx_fail(tx, RfsTxErrStart);
            break;
        }
        uint32_t started = furi_get_tick();
        while(!subghz_devices_is_async_complete_tx(device)) {
            if(tx->cancel) break;
            if(furi_get_tick() - started > furi_ms_to_ticks(RFS_TX_TIMEOUT_MS)) {
                result = rfs_tx_fail(tx, RfsTxErrTimeout);
                break;
            }
            furi_delay_ms(10);
        }
        subghz_devices_stop_async_tx(device);
    } while(false);

    /* Always clean up and return the radio to idle (TX-011). */
    if(transmitter) {
        subghz_transmitter_stop(transmitter);
        subghz_transmitter_free(transmitter);
    }
    if(radio_acquired) rfs_radio_release(tx->radio, tx->device_id);
    if(raw_ff) flipper_format_free(raw_ff);
    if(env) subghz_environment_free(env);
    free(custom_preset);
    furi_string_free(tmp);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return result;
}

static int32_t rfs_tx_thread(void* context) {
    RfsTx* tx = context;
    RfsTxError error = rfs_tx_run(tx);

    RfsTxState final;
    if(error != RfsTxErrNone) {
        final = RfsTxStateFailed;
        char text[40];
        rfs_tx_set_state(tx, final);
        rfs_tx_outcome_text(tx, text, sizeof(text));
        rfs_diag_error(tx->diag, "TX failed: %s", text);
    } else if(tx->cancel) {
        final = RfsTxStateCancelled;
    } else {
        final = RfsTxStateCompleted;
    }
    rfs_tx_set_state(tx, final);

    furi_mutex_acquire(tx->mutex, FuriWaitForever);
    tx->running = false;
    RfsTxDoneCallback callback = tx->callback;
    void* cb_context = tx->context;
    furi_mutex_release(tx->mutex);
    if(callback) callback(cb_context);
    return 0;
}

RfsTx* rfs_tx_alloc(RfsRadio* radio, RfsDiag* diag) {
    RfsTx* tx = malloc(sizeof(RfsTx));
    memset(tx, 0, sizeof(RfsTx));
    tx->radio = radio;
    tx->diag = diag;
    tx->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    tx->thread = furi_thread_alloc_ex("RfsTx", 4096, rfs_tx_thread, tx);
    return tx;
}

void rfs_tx_free(RfsTx* tx) {
    rfs_tx_cancel(tx);
    if(furi_thread_get_state(tx->thread) != FuriThreadStateStopped) {
        furi_thread_join(tx->thread);
    }
    furi_thread_free(tx->thread);
    furi_mutex_free(tx->mutex);
    free(tx);
}

bool rfs_tx_start(
    RfsTx* tx,
    const char* signal_path,
    RfsRadioMode mode,
    RfsTxDoneCallback callback,
    void* context) {
    furi_mutex_acquire(tx->mutex, FuriWaitForever);
    bool busy = tx->running;
    furi_mutex_release(tx->mutex);
    if(busy) return false;

    /* The previous run has finished; reap its thread before restarting it. */
    if(furi_thread_get_state(tx->thread) != FuriThreadStateStopped) {
        furi_thread_join(tx->thread);
    }

    strlcpy(tx->path, signal_path, sizeof(tx->path));
    tx->mode = mode;
    tx->callback = callback;
    tx->context = context;
    tx->cancel = false;
    tx->error = RfsTxErrNone;
    tx->radio_result = RfsRadioOk;
    tx->permission = RfsTxPermAllowed;
    tx->device_id = RfsDeviceInternal;
    tx->state = RfsTxStateValidating;
    tx->running = true;
    furi_thread_start(tx->thread);
    return true;
}

void rfs_tx_cancel(RfsTx* tx) {
    tx->cancel = true;
}

bool rfs_tx_is_running(RfsTx* tx) {
    furi_mutex_acquire(tx->mutex, FuriWaitForever);
    bool running = tx->running;
    furi_mutex_release(tx->mutex);
    return running;
}

RfsTxState rfs_tx_state(RfsTx* tx) {
    furi_mutex_acquire(tx->mutex, FuriWaitForever);
    RfsTxState state = tx->state;
    furi_mutex_release(tx->mutex);
    return state;
}

void rfs_tx_outcome_text(RfsTx* tx, char* out, size_t size) {
    RfsTxState state = rfs_tx_state(tx);
    switch(state) {
    case RfsTxStateCompleted:
        strlcpy(out, "Sent", size);
        return;
    case RfsTxStateCancelled:
        strlcpy(out, "Cancelled", size);
        return;
    case RfsTxStateFailed:
        break;
    default:
        strlcpy(out, "Sending...", size);
        return;
    }
    switch(tx->error) {
    case RfsTxErrFileMissing:
        strlcpy(out, "Signal file missing", size);
        break;
    case RfsTxErrFileInvalid:
        strlcpy(out, "Signal file invalid", size);
        break;
    case RfsTxErrProtocolUnknown:
        strlcpy(out, "Protocol not in firmware", size);
        break;
    case RfsTxErrRollingCode:
        strlcpy(out, "Rolling code: not sent", size);
        break;
    case RfsTxErrNotSendable:
        strlcpy(out, "Firmware can't send this", size);
        break;
    case RfsTxErrRadio:
        if(tx->radio_result == RfsRadioErrTxBlocked) {
            strlcpy(out, rfs_tx_permission_text(tx->permission), size);
        } else {
            strlcpy(out, rfs_radio_result_text(tx->radio_result), size);
        }
        break;
    case RfsTxErrPreset:
        strlcpy(out, "Unknown modulation", size);
        break;
    case RfsTxErrEncoder:
        strlcpy(out, "Encoder failed", size);
        break;
    case RfsTxErrStart:
        strlcpy(out, "Radio refused TX", size);
        break;
    case RfsTxErrTimeout:
        strlcpy(out, "Timed out", size);
        break;
    default:
        strlcpy(out, "Failed", size);
        break;
    }
}

const char* rfs_tx_device_label(RfsTx* tx) {
    return rfs_radio_device_label(tx->device_id);
}
