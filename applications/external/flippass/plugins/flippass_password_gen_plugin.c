#include "flippass_password_gen_plugin.h"

#include "../kdbx/hmac.h"
#include "../kdbx/memzero.h"

#include <furi_hal_random.h>
#include <input/input.h>

#if __has_include(<furi_hal_subghz.h>)
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/cc1101_configs.h>
#define FLIPPASS_PASSWORD_GEN_HAVE_SUBGHZ 1
#else
#define FLIPPASS_PASSWORD_GEN_HAVE_SUBGHZ 0
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_PASSWORD_GEN_DOMAIN "FlipPass password generator v1"

typedef struct {
    bool active;
    bool subghz_async_started;
    bool subghz_available;
    FlipPassPasswordGenPluginRequestV1 request;
    FuriMutex* mutex;
    SHA256_CTX digest;
    uint8_t hw_seed[SHA256_DIGEST_LENGTH];
    uint32_t input_events;
    uint32_t subghz_samples;
    volatile uint32_t subghz_edges;
    volatile uint32_t subghz_edge_mix;
    volatile uint32_t subghz_edge_last_duration;
    uint32_t last_input_tick[InputKeyMAX];
    uint32_t stream_counter;
    uint8_t stream[SHA256_DIGEST_LENGTH];
    size_t stream_offset;
    uint8_t seed[SHA256_DIGEST_LENGTH];
    bool seed_ready;
} FlipPassPasswordGenState;

static FlipPassPasswordGenState flippass_password_gen_state = {0};

static const char flippass_password_gen_lower[] = "abcdefghijklmnopqrstuvwxyz";
static const char flippass_password_gen_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char flippass_password_gen_digits[] = "0123456789";
static const char flippass_password_gen_symbols[] = "!@#$%^&*()-_=+[]{};:,.?/";
static const char flippass_password_gen_hex[] = "0123456789ABCDEF";

static uint32_t flippass_password_gen_rotl32(uint32_t value, uint8_t shift) {
    return (value << shift) | (value >> (32U - shift));
}

static void flippass_password_gen_update_bytes(const uint8_t* data, size_t size) {
    sha256_Update(&flippass_password_gen_state.digest, data, size);
}

static void flippass_password_gen_update_u32(uint32_t value) {
    flippass_password_gen_update_bytes((const uint8_t*)&value, sizeof(value));
}

static void flippass_password_gen_update_tag(const char* tag) {
    flippass_password_gen_update_bytes((const uint8_t*)tag, strlen(tag));
}

static void flippass_password_gen_mix_record(const char* tag, const void* data, size_t size) {
    flippass_password_gen_update_tag(tag);
    flippass_password_gen_update_u32((uint32_t)size);
    if(data != NULL && size > 0U) {
        flippass_password_gen_update_bytes(data, size);
    }
}

static void flippass_password_gen_lock(void) {
    if(flippass_password_gen_state.mutex != NULL) {
        furi_mutex_acquire(flippass_password_gen_state.mutex, FuriWaitForever);
    }
}

static void flippass_password_gen_unlock(void) {
    if(flippass_password_gen_state.mutex != NULL) {
        furi_mutex_release(flippass_password_gen_state.mutex);
    }
}

#if FLIPPASS_PASSWORD_GEN_HAVE_SUBGHZ
static void
    flippass_password_gen_subghz_capture_callback(bool level, uint32_t duration, void* context) {
    UNUSED(context);

    FlipPassPasswordGenState* state = &flippass_password_gen_state;
    const uint32_t count = state->subghz_edges + 1U;
    uint32_t mix = state->subghz_edge_mix ^ duration ^ (level ? 0x9E3779B9U : 0x7F4A7C15U);
    mix ^= count * 0x85EBCA6BU;
    state->subghz_edge_mix = flippass_password_gen_rotl32(mix, 7U);
    state->subghz_edge_last_duration = duration;
    state->subghz_edges = count;
}

static bool flippass_password_gen_start_subghz(void) {
    static const uint32_t frequencies[] = {
        315000000UL,
        433920000UL,
        868350000UL,
        915000000UL,
    };

    uint32_t chosen_frequency = 0U;
    for(size_t index = 0U; index < COUNT_OF(frequencies); index++) {
        if(furi_hal_subghz_is_frequency_valid(frequencies[index])) {
            chosen_frequency = frequencies[index];
            break;
        }
    }

    if(chosen_frequency == 0U) {
        return false;
    }

    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    const uint32_t real_frequency = furi_hal_subghz_set_frequency_and_path(chosen_frequency);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_start_async_rx(flippass_password_gen_subghz_capture_callback, NULL);

    flippass_password_gen_state.subghz_async_started = true;
    flippass_password_gen_state.subghz_available = true;
    flippass_password_gen_mix_record("subghz-start", &real_frequency, sizeof(real_frequency));
    return true;
}

static void flippass_password_gen_stop_subghz(void) {
    if(flippass_password_gen_state.subghz_async_started) {
        furi_hal_subghz_stop_async_rx();
        furi_hal_subghz_sleep();
        flippass_password_gen_state.subghz_async_started = false;
    }
}

static void flippass_password_gen_poll_subghz(uint32_t now_tick) {
    if(!flippass_password_gen_state.subghz_async_started) {
        return;
    }

    const float rssi = furi_hal_subghz_get_rssi();
    const int32_t rssi_q8 = (int32_t)(rssi * 8.0f);
    const uint8_t lqi = furi_hal_subghz_get_lqi();
    const bool pipe_not_empty = furi_hal_subghz_rx_pipe_not_empty();
    const bool crc_valid = pipe_not_empty && furi_hal_subghz_is_rx_data_crc_valid();
    const uint32_t sample[] = {
        now_tick,
        (uint32_t)rssi_q8,
        (uint32_t)lqi,
        pipe_not_empty ? 1U : 0U,
        crc_valid ? 1U : 0U,
        flippass_password_gen_state.subghz_edges,
        flippass_password_gen_state.subghz_edge_mix,
        flippass_password_gen_state.subghz_edge_last_duration,
    };

    flippass_password_gen_mix_record("subghz-sample", sample, sizeof(sample));
    flippass_password_gen_state.subghz_samples++;

    if(pipe_not_empty) {
        furi_hal_subghz_flush_rx();
    }
}
#else
static bool flippass_password_gen_start_subghz(void) {
    return false;
}

static void flippass_password_gen_stop_subghz(void) {
}

static void flippass_password_gen_poll_subghz(uint32_t now_tick) {
    UNUSED(now_tick);
}
#endif

static void flippass_password_gen_reset_sensitive(void) {
    memzero(flippass_password_gen_state.hw_seed, sizeof(flippass_password_gen_state.hw_seed));
    memzero(flippass_password_gen_state.stream, sizeof(flippass_password_gen_state.stream));
    memzero(flippass_password_gen_state.seed, sizeof(flippass_password_gen_state.seed));
    memzero(&flippass_password_gen_state.digest, sizeof(flippass_password_gen_state.digest));
}

static void flippass_password_gen_reset_state(void) {
    FuriMutex* mutex = flippass_password_gen_state.mutex;

    flippass_password_gen_reset_sensitive();
    memset(&flippass_password_gen_state, 0, sizeof(flippass_password_gen_state));
    flippass_password_gen_state.mutex = mutex;
    flippass_password_gen_state.stream_offset = sizeof(flippass_password_gen_state.stream);
}

static bool flippass_password_gen_request_valid(
    const FlipPassPasswordGenPluginRequestV1* request,
    FuriString* error) {
    if(request == NULL || request->api_version != FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass password generator received an invalid request.");
        }
        return false;
    }

    if(request->target == FlipPassPasswordGenTargetNone) {
        if(error != NULL) {
            furi_string_set_str(error, "No editable secret field is selected.");
        }
        return false;
    }

    if(request->length == 0U || request->length > FLIPPASS_PASSWORD_GEN_MAX_LENGTH) {
        if(error != NULL) {
            furi_string_set_str(error, "Password length is out of range.");
        }
        return false;
    }

    if(request->charset > FlipPassPasswordGenCharsetHex) {
        if(error != NULL) {
            furi_string_set_str(error, "Password complexity is unsupported.");
        }
        return false;
    }

    return true;
}

static bool flippass_password_gen_begin(
    const FlipPassPasswordGenPluginRequestV1* request,
    FuriString* error) {
    if(!flippass_password_gen_request_valid(request, error)) {
        return false;
    }

    if(flippass_password_gen_state.mutex == NULL) {
        flippass_password_gen_state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(flippass_password_gen_state.mutex == NULL) {
            if(error != NULL) {
                furi_string_set_str(error, "Not enough RAM to start the password generator.");
            }
            return false;
        }
    }

    flippass_password_gen_lock();
    flippass_password_gen_stop_subghz();
    flippass_password_gen_reset_state();
    flippass_password_gen_state.request = *request;
    flippass_password_gen_state.active = true;

    sha256_Init(&flippass_password_gen_state.digest);
    furi_hal_random_fill_buf(
        flippass_password_gen_state.hw_seed, sizeof(flippass_password_gen_state.hw_seed));
    flippass_password_gen_mix_record(
        "domain", FLIPPASS_PASSWORD_GEN_DOMAIN, strlen(FLIPPASS_PASSWORD_GEN_DOMAIN));
    flippass_password_gen_mix_record("request", request, sizeof(*request));

    const uint32_t start_tick = furi_get_tick();
    const uint32_t random_word = furi_hal_random_get();
    flippass_password_gen_mix_record("start-tick", &start_tick, sizeof(start_tick));
    flippass_password_gen_mix_record("random-word", &random_word, sizeof(random_word));

    if(request->harvest_seconds > 0U) {
        const bool subghz_ok = flippass_password_gen_start_subghz();
        flippass_password_gen_mix_record("subghz-ok", &subghz_ok, sizeof(subghz_ok));
    }

    flippass_password_gen_unlock();
    return true;
}

static bool
    flippass_password_gen_record_input(const FlipPassPasswordGenPluginInputRecordV1* record) {
    if(record == NULL || !flippass_password_gen_state.active) {
        return false;
    }

    flippass_password_gen_lock();
    if(!flippass_password_gen_state.active) {
        flippass_password_gen_unlock();
        return false;
    }

    uint32_t press_duration = 0U;
    if(record->key < InputKeyMAX) {
        if(record->type == InputTypePress) {
            flippass_password_gen_state.last_input_tick[record->key] = record->tick;
        } else if(
            record->type == InputTypeRelease &&
            flippass_password_gen_state.last_input_tick[record->key] != 0U) {
            press_duration =
                record->tick - flippass_password_gen_state.last_input_tick[record->key];
        }
    }

    flippass_password_gen_mix_record("input", record, sizeof(*record));
    flippass_password_gen_mix_record("input-duration", &press_duration, sizeof(press_duration));
    flippass_password_gen_state.input_events++;
    flippass_password_gen_unlock();
    return true;
}

static bool
    flippass_password_gen_poll(uint32_t now_tick, FlipPassPasswordGenPluginStatusV1* status) {
    if(!flippass_password_gen_state.active) {
        return false;
    }

    flippass_password_gen_lock();
    if(!flippass_password_gen_state.active) {
        flippass_password_gen_unlock();
        return false;
    }

    flippass_password_gen_poll_subghz(now_tick);

    if(status != NULL) {
        status->input_events = flippass_password_gen_state.input_events;
        status->subghz_samples = flippass_password_gen_state.subghz_samples;
        status->subghz_edges = flippass_password_gen_state.subghz_edges;
        status->subghz_active = flippass_password_gen_state.subghz_async_started;
    }

    flippass_password_gen_unlock();
    return true;
}

static size_t flippass_password_gen_append_chars(char* out, size_t out_size, const char* chars) {
    size_t out_len = strlen(out);

    for(const char* cursor = chars; cursor != NULL && *cursor != '\0' && out_len + 1U < out_size;
        cursor++) {
        out[out_len++] = *cursor;
    }
    out[out_len] = '\0';
    return out_len;
}

static size_t flippass_password_gen_build_classes(
    const FlipPassPasswordGenPluginRequestV1* request,
    const char** classes,
    size_t max_classes,
    char* combined,
    size_t combined_size) {
    size_t class_count = 0U;
    combined[0] = '\0';

    switch(request->charset) {
    case FlipPassPasswordGenCharsetAlnum:
        classes[class_count++] = flippass_password_gen_lower;
        classes[class_count++] = flippass_password_gen_upper;
        classes[class_count++] = flippass_password_gen_digits;
        break;
    case FlipPassPasswordGenCharsetAlpha:
        classes[class_count++] = flippass_password_gen_lower;
        classes[class_count++] = flippass_password_gen_upper;
        break;
    case FlipPassPasswordGenCharsetSymbols:
        classes[class_count++] = flippass_password_gen_symbols;
        break;
    case FlipPassPasswordGenCharsetNumeric:
        classes[class_count++] = flippass_password_gen_digits;
        break;
    case FlipPassPasswordGenCharsetHex:
        classes[class_count++] = flippass_password_gen_hex;
        break;
    case FlipPassPasswordGenCharsetFull:
    default:
        classes[class_count++] = flippass_password_gen_lower;
        classes[class_count++] = flippass_password_gen_upper;
        classes[class_count++] = flippass_password_gen_digits;
        classes[class_count++] = flippass_password_gen_symbols;
        break;
    }

    if(class_count > max_classes) {
        class_count = max_classes;
    }

    for(size_t index = 0U; index < class_count; index++) {
        flippass_password_gen_append_chars(combined, combined_size, classes[index]);
    }

    return class_count;
}

static void flippass_password_gen_derive_seed(void) {
    uint8_t digest[SHA256_DIGEST_LENGTH];
    uint8_t message[160];
    size_t offset = 0U;

    sha256_Final(&flippass_password_gen_state.digest, digest);

#define APPEND_FIELD(ptr, size)                          \
    do {                                                 \
        const size_t field_size = (size);                \
        if(offset + field_size <= sizeof(message)) {     \
            memcpy(message + offset, (ptr), field_size); \
            offset += field_size;                        \
        }                                                \
    } while(0)

    APPEND_FIELD(FLIPPASS_PASSWORD_GEN_DOMAIN, strlen(FLIPPASS_PASSWORD_GEN_DOMAIN));
    APPEND_FIELD(
        &flippass_password_gen_state.request, sizeof(flippass_password_gen_state.request));
    APPEND_FIELD(digest, sizeof(digest));
    APPEND_FIELD(
        &flippass_password_gen_state.input_events,
        sizeof(flippass_password_gen_state.input_events));
    APPEND_FIELD(
        &flippass_password_gen_state.subghz_samples,
        sizeof(flippass_password_gen_state.subghz_samples));
    APPEND_FIELD(
        (const void*)&flippass_password_gen_state.subghz_edges,
        sizeof(flippass_password_gen_state.subghz_edges));
    APPEND_FIELD(
        (const void*)&flippass_password_gen_state.subghz_edge_mix,
        sizeof(flippass_password_gen_state.subghz_edge_mix));

#undef APPEND_FIELD

    hmac_sha256(
        flippass_password_gen_state.hw_seed,
        sizeof(flippass_password_gen_state.hw_seed),
        message,
        (uint32_t)offset,
        flippass_password_gen_state.seed);
    flippass_password_gen_state.seed_ready = true;
    flippass_password_gen_state.stream_counter = 0U;
    flippass_password_gen_state.stream_offset = sizeof(flippass_password_gen_state.stream);

    memzero(digest, sizeof(digest));
    memzero(message, sizeof(message));
}

static void flippass_password_gen_refill_stream(void) {
    uint8_t message[sizeof(flippass_password_gen_state.stream_counter) + 8U];

    memset(message, 0, sizeof(message));
    memcpy(message, "stream", 6U);
    memcpy(message + 8U, &flippass_password_gen_state.stream_counter, sizeof(uint32_t));
    hmac_sha256(
        flippass_password_gen_state.seed,
        sizeof(flippass_password_gen_state.seed),
        message,
        sizeof(message),
        flippass_password_gen_state.stream);
    flippass_password_gen_state.stream_counter++;
    flippass_password_gen_state.stream_offset = 0U;
    memzero(message, sizeof(message));
}

static uint8_t flippass_password_gen_next_byte(void) {
    if(flippass_password_gen_state.stream_offset >= sizeof(flippass_password_gen_state.stream)) {
        flippass_password_gen_refill_stream();
    }

    return flippass_password_gen_state.stream[flippass_password_gen_state.stream_offset++];
}

static size_t flippass_password_gen_uniform(size_t count) {
    if(count <= 1U) {
        return 0U;
    }

    const uint16_t limit = (uint16_t)(256U - (256U % count));
    uint8_t byte = 0U;
    do {
        byte = flippass_password_gen_next_byte();
    } while(byte >= limit);

    return byte % count;
}

static void flippass_password_gen_shuffle(char* text, size_t length) {
    if(length <= 1U) {
        return;
    }

    for(size_t index = length - 1U; index > 0U; index--) {
        const size_t swap_index = flippass_password_gen_uniform(index + 1U);
        const char tmp = text[index];
        text[index] = text[swap_index];
        text[swap_index] = tmp;
    }
}

static bool flippass_password_gen_generate_password(
    const FlipPassPasswordGenPluginRequestV1* request,
    char out[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U],
    FuriString* error) {
    const char* classes[4] = {0};
    char combined[128];
    size_t out_index = 0U;

    const size_t class_count = flippass_password_gen_build_classes(
        request, classes, COUNT_OF(classes), combined, sizeof(combined));
    const size_t combined_len = strlen(combined);
    if(class_count == 0U || combined_len == 0U) {
        if(error != NULL) {
            furi_string_set_str(error, "Password complexity produced an empty character set.");
        }
        return false;
    }

    if(!flippass_password_gen_state.seed_ready) {
        flippass_password_gen_derive_seed();
    }

    memset(out, 0, FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U);

    if(class_count > 1U && request->length >= class_count) {
        for(size_t class_index = 0U; class_index < class_count; class_index++) {
            const char* cls = classes[class_index];
            out[out_index++] = cls[flippass_password_gen_uniform(strlen(cls))];
        }
    }

    while(out_index < request->length) {
        out[out_index++] = combined[flippass_password_gen_uniform(combined_len)];
    }
    out[out_index] = '\0';

    flippass_password_gen_shuffle(out, out_index);
    return true;
}

static bool
    flippass_password_gen_finish(FlipPassPasswordGenPluginResultV1* result, FuriString* error) {
    if(result == NULL || !flippass_password_gen_state.active) {
        if(error != NULL) {
            furi_string_set_str(error, "Password generation is not active.");
        }
        return false;
    }

    flippass_password_gen_lock();
    if(!flippass_password_gen_state.active) {
        flippass_password_gen_unlock();
        if(error != NULL) {
            furi_string_set_str(error, "Password generation already finished.");
        }
        return false;
    }

    memset(result, 0, sizeof(*result));
    const uint32_t finish_tick = furi_get_tick();
    flippass_password_gen_poll_subghz(finish_tick);
    flippass_password_gen_mix_record("finish-tick", &finish_tick, sizeof(finish_tick));
    flippass_password_gen_stop_subghz();

    const bool ok = flippass_password_gen_generate_password(
        &flippass_password_gen_state.request, result->password, error);
    result->status.input_events = flippass_password_gen_state.input_events;
    result->status.subghz_samples = flippass_password_gen_state.subghz_samples;
    result->status.subghz_edges = flippass_password_gen_state.subghz_edges;
    result->status.subghz_active = false;

    flippass_password_gen_state.active = false;
    flippass_password_gen_reset_state();
    flippass_password_gen_unlock();
    return ok;
}

static void flippass_password_gen_abort(void) {
    FuriMutex* mutex = flippass_password_gen_state.mutex;

    if(flippass_password_gen_state.mutex != NULL) {
        flippass_password_gen_lock();
    }
    flippass_password_gen_stop_subghz();
    flippass_password_gen_state.active = false;
    flippass_password_gen_reset_state();
    if(flippass_password_gen_state.mutex != NULL) {
        flippass_password_gen_unlock();
    }

    if(mutex != NULL) {
        flippass_password_gen_state.mutex = NULL;
        furi_mutex_free(mutex);
    }
}

static const FlipPassPasswordGenPluginV1 flippass_password_gen_plugin = {
    .api_version = FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION,
    .begin = flippass_password_gen_begin,
    .record_input = flippass_password_gen_record_input,
    .poll = flippass_password_gen_poll,
    .finish = flippass_password_gen_finish,
    .abort = flippass_password_gen_abort,
};

static const FlipperAppPluginDescriptor flippass_password_gen_descriptor = {
    .appid = FLIPPASS_PASSWORD_GEN_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION,
    .entry_point = &flippass_password_gen_plugin,
};

const FlipperAppPluginDescriptor* flippass_password_gen_plugin_ep(void) {
    return &flippass_password_gen_descriptor;
}
