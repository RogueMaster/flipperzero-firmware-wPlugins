#include "rfs_capture.h"

#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/protocols/raw.h>
#include <lib/subghz/protocols/base.h>
#include <toolbox/stream/stream.h>

#define RFS_CAPTURE_RAW_PATH        RFS_TMP_DIR "/capture_raw.sub"
#define RFS_RAW_LINE_SAMPLES        512
#define RFS_RAW_MAX_SAMPLES         (RFS_RAW_LINE_SAMPLES * 96)
#define RFS_RAW_MIN_SAMPLES         64
#define RFS_RAW_MIN_PULSE_US        50
#define RFS_RAW_QUIET_MS            700
#define RFS_RAW_MIN_FREE_BYTES      (128 * 1024)
#define RFS_DISCOVERY_PRESET        "AM650"
#define RFS_DISCOVERY_SETTLE_MS     2
#define RFS_DISCOVERY_LEARN_SWEEPS  3
#define RFS_DISCOVERY_CONFIRM_READS 3
/* A transmission must stand this far above the frequency's background noise. */
#define RFS_SIGNAL_MARGIN_DB        10.0f
#define RFS_FLOOR_ALPHA             0.2f
#define RFS_RSSI_UNSET              (-500.0f)

struct RfsCapture {
    RfsRadio* radio;
    RfsDiag* diag;
    RfsCaptureCallback callback;
    void* context;

    RfsCaptureState state;
    RfsDeviceId device_id;
    bool radio_acquired;
    int32_t threshold;
    volatile float rssi;

    /* Discovery */
    FuriThread* discovery_thread;
    volatile bool discovery_stop;
    RfsDiscoverScope scope;
    volatile uint32_t sweep_frequency;
    volatile uint32_t found_frequency;
    float found_floor; /* noise floor measured at found_frequency */

    /* Listening */
    uint32_t frequency;
    char preset[RFS_PRESET_LEN];
    const uint8_t* preset_data;
    size_t preset_size;
    SubGhzEnvironment* env;
    SubGhzReceiver* receiver;
    SubGhzWorker* worker;
    volatile bool decoded;
    volatile uint32_t overruns;
    FlipperFormat* decoded_ff;
    const SubGhzProtocol* decoded_protocol;
    FuriString* decoded_text;
    /* First decode of a transmission waits for a matching repeat before it is trusted, so a
     * frame cut off at the start of listening cannot be saved as the button's code. */
    bool candidate; /* decoded_ff holds an unconfirmed decode */
    uint8_t candidate_hash;
    FuriString* candidate_key; /* "Key" value of the candidate, empty if the protocol has none */
    FuriString* frame_key;

    /* Raw recording, shared with the worker thread under raw_mutex */
    FuriMutex* raw_mutex;
    FlipperFormat* raw_ff;
    int32_t raw_buffer[RFS_RAW_LINE_SAMPLES];
    size_t raw_buffered;
    size_t raw_samples;
    bool raw_armed;
    bool raw_closed;
    bool raw_failed;
    bool raw_ready;
    uint32_t last_above_tick;
    float noise_floor; /* background level while listening */

    RfsCaptureResult result;
    const char* error_text;
};

static void rfs_capture_notify(RfsCapture* capture, RfsCaptureEvent event) {
    if(capture->callback) capture->callback(event, capture->context);
}

/* ---------- Raw recording ---------- */

/* Caller holds raw_mutex. */
static void rfs_raw_flush(RfsCapture* capture) {
    if(!capture->raw_ff || capture->raw_buffered == 0) return;
    if(!flipper_format_write_int32(
           capture->raw_ff, "RAW_Data", capture->raw_buffer, capture->raw_buffered)) {
        capture->raw_failed = true;
        capture->raw_closed = true;
    }
    capture->raw_samples += capture->raw_buffered;
    capture->raw_buffered = 0;
}

/* Caller holds raw_mutex. */
static void rfs_raw_close(RfsCapture* capture) {
    rfs_raw_flush(capture);
    if(capture->raw_ff) {
        flipper_format_file_close(capture->raw_ff);
        flipper_format_free(capture->raw_ff);
        capture->raw_ff = NULL;
    }
    capture->raw_closed = true;
}

/* Caller holds raw_mutex. Discards any recording in progress. */
static void rfs_raw_reset(RfsCapture* capture) {
    if(capture->raw_ff) {
        flipper_format_file_close(capture->raw_ff);
        flipper_format_free(capture->raw_ff);
        capture->raw_ff = NULL;
    }
    capture->raw_buffered = 0;
    capture->raw_samples = 0;
    capture->raw_armed = false;
    capture->raw_closed = false;
    capture->raw_failed = false;
    capture->raw_ready = false;
}

/* Caller holds raw_mutex. Opens the temporary file and writes the RAW header. */
static bool rfs_raw_arm(RfsCapture* capture) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    uint64_t total = 0, free_bytes = 0;
    bool ok = storage_common_fs_info(storage, STORAGE_EXT_PATH_PREFIX, &total, &free_bytes) ==
                  FSE_OK &&
              free_bytes > RFS_RAW_MIN_FREE_BYTES;
    if(!ok) {
        capture->error_text = "SD card is full";
    }
    if(ok) {
        storage_common_remove(storage, RFS_CAPTURE_RAW_PATH);
        capture->raw_ff = flipper_format_file_alloc(storage);
        ok = flipper_format_file_open_always(capture->raw_ff, RFS_CAPTURE_RAW_PATH);
        FuriString* preset = furi_string_alloc();
        rfs_radio_preset_to_file(capture->preset, preset);
        ok = ok && flipper_format_write_header_cstr(capture->raw_ff, "Flipper SubGhz RAW File", 1);
        ok = ok &&
             flipper_format_write_uint32(capture->raw_ff, "Frequency", &capture->frequency, 1);
        ok = ok && flipper_format_write_string(capture->raw_ff, "Preset", preset);
        if(ok && furi_string_cmp_str(preset, "FuriHalSubGhzPresetCustom") == 0) {
            ok = flipper_format_write_string_cstr(
                capture->raw_ff, "Custom_preset_module", "CC1101");
            ok = ok && flipper_format_write_hex(
                           capture->raw_ff,
                           "Custom_preset_data",
                           capture->preset_data,
                           capture->preset_size);
        }
        ok = ok && flipper_format_write_string_cstr(
                       capture->raw_ff, "Protocol", SUBGHZ_PROTOCOL_RAW_NAME);
        furi_string_free(preset);
        if(!ok) {
            capture->error_text = "Can't write recording";
            flipper_format_free(capture->raw_ff);
            capture->raw_ff = NULL;
        }
    }
    furi_record_close(RECORD_STORAGE);
    capture->raw_armed = ok;
    capture->raw_failed = !ok;
    return ok;
}

/* ---------- Worker callbacks (worker thread) ---------- */

static void rfs_capture_rx_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    RfsCapture* capture = context;
    if(capture->decoded) return;

    uint8_t hash = subghz_protocol_decoder_base_get_hash_data(decoder_base);

    SubGhzRadioPreset preset = {
        .name = furi_string_alloc_set(capture->preset),
        .frequency = capture->frequency,
        .data = (uint8_t*)capture->preset_data,
        .data_size = capture->preset_size,
    };
    stream_clean(flipper_format_get_raw_stream(capture->decoded_ff));
    SubGhzProtocolStatus status =
        subghz_protocol_decoder_base_serialize(decoder_base, capture->decoded_ff, &preset);
    furi_string_free(preset.name);
    if(status != SubGhzProtocolStatusOk) return;

    /* Compare the decoded key itself; protocols without a Key field fall back to the decoder's
     * data hash. Timing values such as TE vary between frames and are not compared. */
    flipper_format_rewind(capture->decoded_ff);
    if(!flipper_format_read_string(capture->decoded_ff, "Key", capture->frame_key)) {
        furi_string_reset(capture->frame_key);
    }
    flipper_format_rewind(capture->decoded_ff);
    bool same_code = furi_string_empty(capture->frame_key) ?
                         capture->candidate_hash == hash :
                         furi_string_equal(capture->candidate_key, capture->frame_key);
    bool confirms = capture->candidate && capture->decoded_protocol == decoder_base->protocol &&
                    same_code;

    furi_string_reset(capture->decoded_text);
    subghz_protocol_decoder_base_get_string(decoder_base, capture->decoded_text);
    capture->decoded_protocol = decoder_base->protocol;
    if(!confirms) {
        /* New or different code: remember it and wait for the next frame. */
        capture->candidate = true;
        capture->candidate_hash = hash;
        furi_string_set(capture->candidate_key, capture->frame_key);
        return;
    }
    capture->decoded = true;
    rfs_capture_notify(capture, RfsCaptureEventDecoded);
}

static void rfs_capture_pair_callback(void* context, bool level, uint32_t duration) {
    RfsCapture* capture = context;
    if(capture->decoded) return;
    subghz_receiver_decode(capture->receiver, level, duration);

    if(!capture->raw_armed || capture->raw_closed) return;
    if(duration < RFS_RAW_MIN_PULSE_US) return;
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    if(capture->raw_armed && !capture->raw_closed) {
        int32_t sample = level ? (int32_t)duration : -(int32_t)duration;
        capture->raw_buffer[capture->raw_buffered++] = sample;
        if(capture->raw_buffered >= RFS_RAW_LINE_SAMPLES) rfs_raw_flush(capture);
        if(capture->raw_samples >= RFS_RAW_MAX_SAMPLES) rfs_raw_close(capture);
    }
    furi_mutex_release(capture->raw_mutex);
}

static void rfs_capture_overrun_callback(void* context) {
    RfsCapture* capture = context;
    capture->overruns++;
    subghz_receiver_reset(capture->receiver);
}

/* ---------- Discovery (discovery thread) ---------- */

static float rfs_discovery_measure(const SubGhzDevice* device, uint32_t frequency) {
    subghz_devices_idle(device);
    subghz_devices_set_frequency(device, frequency);
    subghz_devices_set_rx(device);
    furi_delay_ms(RFS_DISCOVERY_SETTLE_MS);
    return subghz_devices_get_rssi(device);
}

static int32_t rfs_capture_discovery_thread(void* context) {
    RfsCapture* capture = context;
    const SubGhzDevice* device = rfs_radio_device(capture->radio, capture->device_id);
    const uint8_t* data = NULL;
    size_t size = 0;
    if(!rfs_radio_preset_data(capture->radio, RFS_DISCOVERY_PRESET, &data, &size)) return 0;

    subghz_devices_idle(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetCustom, (uint8_t*)data);
    size_t count = rfs_radio_freq_count(capture->radio, capture->scope);

    /* Per-frequency noise floor, learned over the first sweeps and then tracked slowly, so a
     * frequency with constant local interference does not count as a transmission. */
    float* floor = malloc(sizeof(float) * (count ? count : 1));
    for(size_t i = 0; i < count; i++)
        floor[i] = RFS_RSSI_UNSET;
    uint32_t sweeps = 0;

    while(!capture->discovery_stop) {
        float best = -200.0f;
        float peak = -200.0f;
        size_t best_index = 0;
        uint32_t best_frequency = 0;
        bool any = false;
        for(size_t i = 0; i < count && !capture->discovery_stop; i++) {
            uint32_t frequency = rfs_radio_freq_get(capture->radio, capture->scope, i);
            if(!subghz_devices_is_frequency_valid(device, frequency)) continue;
            any = true;
            capture->sweep_frequency = frequency;
            float rssi = rfs_discovery_measure(device, frequency);
            if(rssi > peak) peak = rssi;

            bool learning = sweeps < RFS_DISCOVERY_LEARN_SWEEPS;
            if(floor[i] <= RFS_RSSI_UNSET) {
                floor[i] = rssi;
            } else if(learning) {
                if(rssi < floor[i]) floor[i] = rssi;
            }
            bool candidate = !learning && rssi >= (float)capture->threshold &&
                             rssi >= floor[i] + RFS_SIGNAL_MARGIN_DB;
            if(candidate && rssi > best) {
                best = rssi;
                best_index = i;
                best_frequency = frequency;
            }
            if(!candidate && !learning) {
                floor[i] = floor[i] * (1.0f - RFS_FLOOR_ALPHA) + rssi * RFS_FLOOR_ALPHA;
            }
        }
        if(!any) {
            capture->error_text = "No usable frequencies";
            rfs_capture_notify(capture, RfsCaptureEventError);
            break;
        }
        sweeps++;
        capture->rssi = peak;

        if(best_frequency) {
            /* Confirm: a real transmission is still there a few milliseconds later. */
            float confirm = -200.0f;
            for(uint8_t n = 0; n < RFS_DISCOVERY_CONFIRM_READS; n++) {
                float rssi = rfs_discovery_measure(device, best_frequency);
                if(rssi > confirm) confirm = rssi;
                furi_delay_ms(3);
            }
            if(confirm >= (float)capture->threshold &&
               confirm >= floor[best_index] + RFS_SIGNAL_MARGIN_DB) {
                capture->found_floor = floor[best_index];
                capture->found_frequency = best_frequency;
                rfs_capture_notify(capture, RfsCaptureEventFrequencyFound);
                break;
            }
        }
    }
    free(floor);
    subghz_devices_idle(device);
    return 0;
}

static void rfs_capture_stop_discovery(RfsCapture* capture) {
    if(furi_thread_get_state(capture->discovery_thread) != FuriThreadStateStopped) {
        capture->discovery_stop = true;
        furi_thread_join(capture->discovery_thread);
    }
}

/* ---------- Lifecycle ---------- */

RfsCapture* rfs_capture_alloc(RfsRadio* radio, RfsDiag* diag) {
    RfsCapture* capture = malloc(sizeof(RfsCapture));
    memset(capture, 0, sizeof(RfsCapture));
    capture->radio = radio;
    capture->diag = diag;
    capture->raw_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    capture->discovery_thread =
        furi_thread_alloc_ex("RfsDiscover", 2048, rfs_capture_discovery_thread, capture);
    capture->decoded_ff = flipper_format_string_alloc();
    capture->decoded_text = furi_string_alloc();
    capture->candidate_key = furi_string_alloc();
    capture->frame_key = furi_string_alloc();
    capture->result.text = furi_string_alloc();
    return capture;
}

void rfs_capture_free(RfsCapture* capture) {
    rfs_capture_stop(capture);
    rfs_capture_discard(capture);
    furi_thread_free(capture->discovery_thread);
    furi_mutex_free(capture->raw_mutex);
    flipper_format_free(capture->decoded_ff);
    furi_string_free(capture->decoded_text);
    furi_string_free(capture->candidate_key);
    furi_string_free(capture->frame_key);
    furi_string_free(capture->result.text);
    free(capture);
}

void rfs_capture_set_callback(RfsCapture* capture, RfsCaptureCallback callback, void* context) {
    capture->callback = callback;
    capture->context = context;
}

static RfsRadioResult rfs_capture_take_radio(RfsCapture* capture, RfsDeviceId device) {
    if(capture->radio_acquired && capture->device_id == device) return RfsRadioOk;
    if(capture->radio_acquired) {
        rfs_radio_release(capture->radio, capture->device_id);
        capture->radio_acquired = false;
    }
    RfsRadioResult result = rfs_radio_acquire(capture->radio, device);
    if(result == RfsRadioOk) {
        capture->radio_acquired = true;
        capture->device_id = device;
    }
    return result;
}

static void rfs_capture_stop_listen(RfsCapture* capture) {
    if(capture->worker) {
        const SubGhzDevice* device = rfs_radio_device(capture->radio, capture->device_id);
        if(subghz_worker_is_running(capture->worker)) {
            subghz_worker_stop(capture->worker);
            subghz_devices_stop_async_rx(device);
        }
        subghz_devices_idle(device);
        subghz_worker_free(capture->worker);
        capture->worker = NULL;
    }
    if(capture->receiver) {
        subghz_receiver_free(capture->receiver);
        capture->receiver = NULL;
    }
    if(capture->env) {
        subghz_environment_free(capture->env);
        capture->env = NULL;
    }
}

static void rfs_capture_clear_result(RfsCapture* capture) {
    capture->result.valid = false;
    capture->result.decoded = false;
    capture->result.raw_samples = 0;
    furi_string_reset(capture->result.text);
    capture->decoded = false;
    capture->candidate = false;
    capture->decoded_protocol = NULL;
    capture->error_text = NULL;
}

RfsRadioResult rfs_capture_start_discovery(
    RfsCapture* capture,
    RfsDeviceId device,
    RfsDiscoverScope scope,
    int32_t rssi_threshold) {
    rfs_capture_stop(capture);
    rfs_capture_discard(capture);
    RfsRadioResult result = rfs_capture_take_radio(capture, device);
    if(result != RfsRadioOk) return result;

    capture->scope = scope;
    capture->threshold = rssi_threshold;
    capture->found_frequency = 0;
    capture->found_floor = RFS_RSSI_UNSET;
    capture->sweep_frequency = 0;
    capture->rssi = -200.0f;
    capture->discovery_stop = false;
    capture->state = RfsCaptureStateDiscovering;
    furi_thread_start(capture->discovery_thread);
    return RfsRadioOk;
}

RfsRadioResult rfs_capture_start_listen(
    RfsCapture* capture,
    RfsDeviceId device,
    uint32_t frequency,
    const char* preset,
    int32_t rssi_threshold) {
    rfs_capture_stop_discovery(capture);
    rfs_capture_stop_listen(capture);
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    rfs_raw_reset(capture);
    furi_mutex_release(capture->raw_mutex);
    rfs_capture_clear_result(capture);

    RfsRadioResult result = rfs_capture_take_radio(capture, device);
    if(result != RfsRadioOk) return result;

    const SubGhzDevice* radio_device = rfs_radio_device(capture->radio, device);
    if(!subghz_devices_is_frequency_valid(radio_device, frequency)) {
        return RfsRadioErrFrequency;
    }
    strlcpy(capture->preset, preset, sizeof(capture->preset));
    if(!rfs_radio_preset_data(
           capture->radio, capture->preset, &capture->preset_data, &capture->preset_size)) {
        strlcpy(capture->preset, RFS_DISCOVERY_PRESET, sizeof(capture->preset));
        rfs_radio_preset_data(
            capture->radio, capture->preset, &capture->preset_data, &capture->preset_size);
    }
    /* Start from the floor discovery measured, so a remote still transmitting right after
     * discovery is not mistaken for background noise. */
    capture->noise_floor =
        (frequency == capture->found_frequency && capture->found_floor > -200.0f) ?
            capture->found_floor :
            RFS_RSSI_UNSET;
    capture->frequency = frequency;
    capture->threshold = rssi_threshold;
    capture->overruns = 0;
    capture->rssi = -200.0f;

    capture->env = rfs_protocol_env_alloc();
    capture->receiver = subghz_receiver_alloc_init(capture->env);
    subghz_receiver_set_filter(capture->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(capture->receiver, rfs_capture_rx_callback, capture);

    capture->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(capture->worker, rfs_capture_overrun_callback);
    subghz_worker_set_pair_callback(capture->worker, rfs_capture_pair_callback);
    subghz_worker_set_context(capture->worker, capture);

    subghz_devices_idle(radio_device);
    subghz_devices_load_preset(
        radio_device, FuriHalSubGhzPresetCustom, (uint8_t*)capture->preset_data);
    capture->frequency = subghz_devices_set_frequency(radio_device, frequency);
    subghz_devices_flush_rx(radio_device);
    subghz_devices_set_rx(radio_device);
    subghz_devices_start_async_rx(radio_device, subghz_worker_rx_callback, capture->worker);
    subghz_worker_start(capture->worker);

    capture->state = RfsCaptureStateListening;
    rfs_diag_info(
        capture->diag,
        "Listening %lu Hz %s on %s",
        (unsigned long)capture->frequency,
        capture->preset,
        rfs_radio_device_label(device));
    return RfsRadioOk;
}

static void rfs_capture_build_result(RfsCapture* capture) {
    RfsCaptureResult* result = &capture->result;
    furi_string_reset(result->text);
    result->frequency = capture->frequency;
    strlcpy(result->preset, capture->preset, sizeof(result->preset));
    if((capture->decoded || capture->candidate) && capture->decoded_protocol) {
        /* An unconfirmed decode is only used when the remote sent a single frame. */
        result->valid = true;
        result->decoded = true;
        result->confirmed = capture->decoded;
        result->kind = rfs_protocol_kind(capture->decoded_protocol);
        strlcpy(result->protocol, capture->decoded_protocol->name, sizeof(result->protocol));
        furi_string_set(result->text, capture->decoded_text);
        result->raw_samples = 0;
    } else if(capture->raw_ready) {
        result->valid = true;
        result->decoded = false;
        result->confirmed = false;
        result->kind = RfsKindRaw;
        strlcpy(result->protocol, SUBGHZ_PROTOCOL_RAW_NAME, sizeof(result->protocol));
        result->raw_samples = capture->raw_samples;
        furi_string_printf(
            result->text,
            "No known protocol decoded.\nRaw recording: %u samples.",
            (unsigned)capture->raw_samples);
    } else {
        result->valid = false;
    }
}

void rfs_capture_stop(RfsCapture* capture) {
    rfs_capture_stop_discovery(capture);
    rfs_capture_stop_listen(capture);
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    if(capture->raw_ff) {
        if(capture->raw_samples + capture->raw_buffered >= RFS_RAW_MIN_SAMPLES &&
           !capture->raw_failed) {
            rfs_raw_close(capture);
            capture->raw_ready = true;
        } else {
            rfs_raw_reset(capture);
        }
    }
    furi_mutex_release(capture->raw_mutex);
    if(capture->radio_acquired) {
        rfs_radio_release(capture->radio, capture->device_id);
        capture->radio_acquired = false;
    }
    if(capture->state != RfsCaptureStateIdle) {
        rfs_capture_build_result(capture);
        capture->state = capture->result.valid ? RfsCaptureStateDone : RfsCaptureStateIdle;
    }
}

void rfs_capture_tick(RfsCapture* capture) {
    if(capture->state != RfsCaptureStateListening || !capture->radio_acquired) return;
    const SubGhzDevice* device = rfs_radio_device(capture->radio, capture->device_id);
    float rssi = subghz_devices_get_rssi(device);
    capture->rssi = rssi;
    if(capture->decoded) return;

    uint32_t now = furi_get_tick();
    bool ready = false;
    if(capture->noise_floor <= RFS_RSSI_UNSET) capture->noise_floor = rssi;
    /* A signal must clear both the trigger level and the local noise by a margin. */
    bool above = rssi >= (float)capture->threshold &&
                 rssi >= capture->noise_floor + RFS_SIGNAL_MARGIN_DB;
    if(!above && !capture->raw_armed) {
        capture->noise_floor =
            capture->noise_floor * (1.0f - RFS_FLOOR_ALPHA) + rssi * RFS_FLOOR_ALPHA;
    }
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    if(above) {
        capture->last_above_tick = now;
        if(!capture->raw_armed && !capture->raw_ready && !capture->raw_failed) {
            rfs_raw_arm(capture);
            if(capture->raw_failed && capture->error_text) {
                rfs_diag_error(capture->diag, "Capture: %s", capture->error_text);
            }
        }
    } else if(
        capture->raw_armed && !capture->raw_ready &&
        now - capture->last_above_tick > furi_ms_to_ticks(RFS_RAW_QUIET_MS)) {
        /* The transmission has ended. Keep it if it holds enough data, else treat it as noise. */
        if(capture->raw_samples + capture->raw_buffered >= RFS_RAW_MIN_SAMPLES &&
           !capture->raw_failed) {
            rfs_raw_close(capture);
            capture->raw_ready = true;
            ready = true;
        } else {
            rfs_raw_reset(capture);
        }
    }
    if(capture->raw_armed && capture->raw_closed && !capture->raw_ready && !capture->raw_failed) {
        /* Maximum length reached in the worker thread. */
        rfs_raw_close(capture);
        capture->raw_ready = true;
        ready = true;
    }
    furi_mutex_release(capture->raw_mutex);
    if(ready) rfs_capture_notify(capture, RfsCaptureEventRawReady);
}

bool rfs_capture_finish_raw(RfsCapture* capture) {
    bool ready = false;
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    if(capture->raw_ready) {
        ready = true;
    } else if(
        capture->raw_ff && capture->raw_samples + capture->raw_buffered >= RFS_RAW_MIN_SAMPLES &&
        !capture->raw_failed) {
        rfs_raw_close(capture);
        capture->raw_ready = true;
        ready = true;
    }
    furi_mutex_release(capture->raw_mutex);
    return ready;
}

RfsCaptureState rfs_capture_state(RfsCapture* capture) {
    return capture->state;
}

float rfs_capture_rssi(RfsCapture* capture) {
    return capture->rssi;
}

uint32_t rfs_capture_frequency(RfsCapture* capture) {
    return capture->state == RfsCaptureStateDiscovering ? capture->found_frequency :
                                                          capture->frequency;
}

uint32_t rfs_capture_sweep_frequency(RfsCapture* capture) {
    return capture->sweep_frequency;
}

size_t rfs_capture_raw_samples(RfsCapture* capture) {
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    size_t samples = capture->raw_samples + capture->raw_buffered;
    furi_mutex_release(capture->raw_mutex);
    return samples;
}

bool rfs_capture_raw_active(RfsCapture* capture) {
    return capture->raw_armed && !capture->raw_closed;
}

uint32_t rfs_capture_overruns(RfsCapture* capture) {
    return capture->overruns;
}

RfsDeviceId rfs_capture_device(RfsCapture* capture) {
    return capture->device_id;
}

const RfsCaptureResult* rfs_capture_result(RfsCapture* capture) {
    return &capture->result;
}

bool rfs_capture_result_key(RfsCapture* capture, FuriString* out) {
    if(!capture->result.valid || !capture->result.decoded) return false;
    flipper_format_rewind(capture->decoded_ff);
    bool ok = flipper_format_read_string(capture->decoded_ff, "Key", out);
    flipper_format_rewind(capture->decoded_ff);
    return ok;
}

bool rfs_capture_save(RfsCapture* capture, Storage* storage, const char* dest_path) {
    if(!capture->result.valid) return false;
    bool ok = false;
    if(capture->result.decoded) {
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_always(ff, dest_path)) {
            Stream* src = flipper_format_get_raw_stream(capture->decoded_ff);
            stream_rewind(src);
            ok = stream_copy_full(src, flipper_format_get_raw_stream(ff)) == stream_size(src);
        }
        flipper_format_free(ff);
    } else {
        storage_common_remove(storage, dest_path);
        ok = storage_common_rename(storage, RFS_CAPTURE_RAW_PATH, dest_path) == FSE_OK;
        if(!ok) {
            ok = storage_common_copy(storage, RFS_CAPTURE_RAW_PATH, dest_path) == FSE_OK;
        }
    }
    if(!ok) storage_common_remove(storage, dest_path);
    return ok;
}

void rfs_capture_discard(RfsCapture* capture) {
    furi_mutex_acquire(capture->raw_mutex, FuriWaitForever);
    rfs_raw_reset(capture);
    furi_mutex_release(capture->raw_mutex);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, RFS_CAPTURE_RAW_PATH);
    furi_record_close(RECORD_STORAGE);
    rfs_capture_clear_result(capture);
    capture->state = RfsCaptureStateIdle;
}

const char* rfs_capture_error_text(RfsCapture* capture) {
    return capture->error_text ? capture->error_text : "Capture failed";
}
