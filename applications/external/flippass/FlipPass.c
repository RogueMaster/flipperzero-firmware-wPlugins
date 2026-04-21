/**
 * @file FlipPass.c
 * @brief Main application file for FlipPass.
 *
 * This file contains the core logic for the FlipPass application, including
 * app allocation, deallocation, and the main entry point. It orchestrates
 * the application's lifecycle and manages the view dispatcher and scene
 * manager.
 */
#include "flippass.h"
#include "flippass_db.h"
#include "flippass_rpc.h"
#include "scenes/flippass_db_browser_view.h"
#include "scenes/flippass_progress_view.h"
#include <flipper_format/flipper_format.h>
#include <input/input.h>
#include <stdio.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TAG                              "FlipPass"
#define FLIPPASS_SYSTEM_LOG_MAX_BYTES    (24U * 1024U)
#define FLIPPASS_TYPING_BACK_SUPPRESS_MS 250U
#define FLIPPASS_SECURE_STORE_SEED       "FlipPass sealed storage v1"

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG && FLIPPASS_ENABLE_LOGS
#define FLIPPASS_STARTUP_LOG(app, ...) FLIPPASS_LOG_EVENT((app), __VA_ARGS__)
#else
#define FLIPPASS_STARTUP_LOG(app, ...) \
    do {                               \
        UNUSED(app);                   \
    } while(0)
#endif

#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE
static uint32_t flippass_system_log_capture_suspend_depth = 0U;
#endif

enum {
    FlipPassCustomEventExit = 0x80000000U,
};

static bool flippass_keyboard_layout_is_valid(const char* path);
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE
static void flippass_system_log_capture_init(App* app);
static void flippass_system_log_capture_deinit(App* app);
#else
static void flippass_system_log_capture_init(App* app);
static void flippass_system_log_capture_deinit(App* app);
#endif

static void flippass_input_events_callback(const void* message, void* context) {
    const InputEvent* event = message;
    App* app = context;

    if(event == NULL || app == NULL) {
        return;
    }

    if(event->key == InputKeyBack && app->typing_active &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app->typing_cancel_requested = true;
        app->typing_cancel_back_tick = furi_get_tick();
        return;
    }

    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassCustomEventExit);
    }
}

static bool flippass_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    App* app = context;

    if(event == FlipPassCustomEventExit) {
        flippass_request_exit(app);
        return true;
    }

    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flippass_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;

    if(app->typing_active) {
        app->typing_cancel_requested = true;
        if(app->typing_cancel_back_tick == 0U) {
            app->typing_cancel_back_tick = furi_get_tick();
        }
        return true;
    }

    if(flippass_typing_consume_pending_back(app)) {
        return true;
    }

    return scene_manager_handle_back_event(app->scene_manager);
}

static bool flippass_db_browser_back_filter(void* context) {
    App* app = context;

    return app != NULL && flippass_typing_consume_pending_back(app);
}

static bool flippass_secure_storage_compose_key(
    char* out_key,
    size_t out_key_size,
    const char* key_prefix,
    const char* suffix) {
    furi_assert(out_key);
    furi_assert(key_prefix);
    furi_assert(suffix);

    const int written = snprintf(out_key, out_key_size, "%s_%s", key_prefix, suffix);
    return written > 0 && (size_t)written < out_key_size;
}

static void flippass_secure_storage_derive_keys(
    const char* key_prefix,
    uint8_t enc_key[32],
    uint8_t mac_key[32]) {
    uint8_t hash[64];
    SHA512_CTX ctx;

    furi_assert(key_prefix);
    furi_assert(enc_key);
    furi_assert(mac_key);

    sha512_Init(&ctx);
    sha512_Update(
        &ctx, (const uint8_t*)FLIPPASS_SECURE_STORE_SEED, strlen(FLIPPASS_SECURE_STORE_SEED));
    sha512_Update(&ctx, (const uint8_t*)key_prefix, strlen(key_prefix));
    sha512_Final(&ctx, hash);
    memcpy(enc_key, hash, 32U);
    memcpy(mac_key, hash + 32U, 32U);
    memzero(hash, sizeof(hash));
}

static void flippass_secure_storage_xor(
    uint8_t* data,
    size_t data_size,
    const uint8_t enc_key[32],
    const uint8_t nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE]) {
    uint8_t keystream[SHA256_DIGEST_LENGTH];
    size_t offset = 0U;
    uint32_t counter = 0U;

    furi_assert(data);
    furi_assert(enc_key);
    furi_assert(nonce);

    while(offset < data_size) {
        uint8_t counter_le[4];
        SHA256_CTX ctx;
        counter_le[0] = (uint8_t)(counter & 0xFFU);
        counter_le[1] = (uint8_t)((counter >> 8U) & 0xFFU);
        counter_le[2] = (uint8_t)((counter >> 16U) & 0xFFU);
        counter_le[3] = (uint8_t)((counter >> 24U) & 0xFFU);
        sha256_Init(&ctx);
        sha256_Update(&ctx, enc_key, 32U);
        sha256_Update(&ctx, nonce, FLIPPASS_SECURE_VALUE_NONCE_SIZE);
        sha256_Update(&ctx, counter_le, sizeof(counter_le));
        sha256_Final(&ctx, keystream);

        const size_t chunk = ((data_size - offset) < sizeof(keystream)) ? (data_size - offset) :
                                                                          sizeof(keystream);
        for(size_t index = 0U; index < chunk; index++) {
            data[offset + index] ^= keystream[index];
        }

        offset += chunk;
        counter++;
    }

    memzero(keystream, sizeof(keystream));
}

static void flippass_secure_storage_mac(
    const char* key_prefix,
    const uint8_t mac_key[32],
    const uint8_t nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE],
    const uint8_t* ciphertext,
    size_t ciphertext_size,
    uint8_t mac[FLIPPASS_SECURE_VALUE_MAC_SIZE]) {
    HMAC_SHA256_CTX ctx;

    furi_assert(key_prefix);
    furi_assert(mac_key);
    furi_assert(nonce);
    furi_assert(mac);

    hmac_sha256_Init(&ctx, mac_key, 32U);
    hmac_sha256_Update(&ctx, (const uint8_t*)key_prefix, strlen(key_prefix));
    hmac_sha256_Update(&ctx, nonce, FLIPPASS_SECURE_VALUE_NONCE_SIZE);
    if(ciphertext != NULL && ciphertext_size > 0U) {
        hmac_sha256_Update(&ctx, ciphertext, ciphertext_size);
    }
    hmac_sha256_Final(&ctx, mac);
}

bool flippass_secure_delete_file_with_storage(Storage* storage, const char* path) {
    if(storage == NULL || path == NULL || !storage_file_exists(storage, path)) {
        return true;
    }

    File* file = storage_file_alloc(storage);
    if(file == NULL) {
        return false;
    }

    bool ok = true;
    if(storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        uint64_t remaining = storage_file_size(file);
        uint8_t wipe[256];
        memset(wipe, 0, sizeof(wipe));
        storage_file_seek(file, 0U, true);

        while(remaining > 0U && ok) {
            const size_t chunk = (remaining > sizeof(wipe)) ? sizeof(wipe) : (size_t)remaining;
            ok = storage_file_write(file, wipe, chunk) == chunk;
            remaining -= chunk;
        }

        if(ok) {
            storage_file_sync(file);
            storage_file_seek(file, 0U, true);
            storage_file_truncate(file);
        }

        storage_file_close(file);
    } else {
        ok = false;
    }

    storage_file_free(file);
    storage_simply_remove(storage, path);
    return ok;
}

bool flippass_secure_delete_file(const char* path) {
    if(path == NULL) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool ok = flippass_secure_delete_file_with_storage(storage, path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool flippass_secure_write_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    const char* value) {
    uint8_t enc_key[32];
    uint8_t mac_key[32];
    uint8_t nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE];
    uint8_t mac[FLIPPASS_SECURE_VALUE_MAC_SIZE];
    uint8_t* ciphertext = NULL;
    char nonce_key[48];
    char cipher_key[48];
    char mac_field_key[48];
    bool ok = false;

    if(file == NULL || key_prefix == NULL) {
        return false;
    }

    if(value == NULL || value[0] == '\0') {
        return true;
    }

    const size_t value_len = strlen(value);
    if(value_len > UINT16_MAX) {
        return false;
    }

    if(!flippass_secure_storage_compose_key(nonce_key, sizeof(nonce_key), key_prefix, "nonce") ||
       !flippass_secure_storage_compose_key(cipher_key, sizeof(cipher_key), key_prefix, "cipher") ||
       !flippass_secure_storage_compose_key(
           mac_field_key, sizeof(mac_field_key), key_prefix, "mac")) {
        return false;
    }

    ciphertext = malloc(value_len);
    if(ciphertext == NULL) {
        return false;
    }

    memcpy(ciphertext, value, value_len);
    furi_hal_random_fill_buf(nonce, sizeof(nonce));
    flippass_secure_storage_derive_keys(key_prefix, enc_key, mac_key);
    flippass_secure_storage_xor(ciphertext, value_len, enc_key, nonce);
    flippass_secure_storage_mac(key_prefix, mac_key, nonce, ciphertext, value_len, mac);

    ok = flipper_format_write_hex(file, nonce_key, nonce, sizeof(nonce)) &&
         flipper_format_write_hex(file, cipher_key, ciphertext, (uint16_t)value_len) &&
         flipper_format_write_hex(file, mac_field_key, mac, sizeof(mac));

    memzero(enc_key, sizeof(enc_key));
    memzero(mac_key, sizeof(mac_key));
    memzero(nonce, sizeof(nonce));
    memzero(mac, sizeof(mac));
    memzero(ciphertext, value_len);
    free(ciphertext);
    return ok;
}

bool flippass_secure_read_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    FuriString* out_value) {
    uint8_t enc_key[32];
    uint8_t mac_key[32];
    uint8_t nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE];
    uint8_t mac[FLIPPASS_SECURE_VALUE_MAC_SIZE];
    uint8_t expected_mac[FLIPPASS_SECURE_VALUE_MAC_SIZE];
    uint32_t cipher_count = 0U;
    uint8_t* ciphertext = NULL;
    char nonce_key[48];
    char cipher_key[48];
    char mac_field_key[48];
    bool ok = false;

    if(file == NULL || key_prefix == NULL || out_value == NULL) {
        return false;
    }

    if(!flippass_secure_storage_compose_key(nonce_key, sizeof(nonce_key), key_prefix, "nonce") ||
       !flippass_secure_storage_compose_key(cipher_key, sizeof(cipher_key), key_prefix, "cipher") ||
       !flippass_secure_storage_compose_key(
           mac_field_key, sizeof(mac_field_key), key_prefix, "mac")) {
        return false;
    }

    furi_string_reset(out_value);
    flipper_format_rewind(file);
    if(!flipper_format_get_value_count(file, cipher_key, &cipher_count) || cipher_count == 0U ||
       cipher_count > UINT16_MAX) {
        return false;
    }

    ciphertext = malloc(cipher_count + 1U);
    if(ciphertext == NULL) {
        return false;
    }

    flipper_format_rewind(file);
    if(!flipper_format_read_hex(file, nonce_key, nonce, sizeof(nonce))) {
        goto cleanup;
    }

    flipper_format_rewind(file);
    if(!flipper_format_read_hex(file, cipher_key, ciphertext, (uint16_t)cipher_count)) {
        goto cleanup;
    }

    flipper_format_rewind(file);
    if(!flipper_format_read_hex(file, mac_field_key, mac, sizeof(mac))) {
        goto cleanup;
    }

    flippass_secure_storage_derive_keys(key_prefix, enc_key, mac_key);
    flippass_secure_storage_mac(
        key_prefix, mac_key, nonce, ciphertext, cipher_count, expected_mac);
    if(memcmp(mac, expected_mac, sizeof(mac)) != 0) {
        goto cleanup;
    }

    flippass_secure_storage_xor(ciphertext, cipher_count, enc_key, nonce);
    ciphertext[cipher_count] = '\0';
    furi_string_set_str(out_value, (const char*)ciphertext);
    ok = true;

cleanup:
    memzero(enc_key, sizeof(enc_key));
    memzero(mac_key, sizeof(mac_key));
    memzero(nonce, sizeof(nonce));
    memzero(mac, sizeof(mac));
    memzero(expected_mac, sizeof(expected_mac));
    if(ciphertext != NULL) {
        memzero(ciphertext, cipher_count + 1U);
        free(ciphertext);
    }
    return ok;
}

void flippass_request_exit(App* app) {
    furi_assert(app);
    scene_manager_stop(app->scene_manager);
    view_dispatcher_stop(app->view_dispatcher);
}

void flippass_typing_begin(App* app) {
    furi_assert(app);

    app->typing_active = true;
    app->typing_cancel_requested = false;
    app->typing_cancel_back_tick = 0U;
}

void flippass_typing_end(App* app) {
    furi_assert(app);

    app->typing_active = false;
    app->typing_cancel_requested = false;
}

bool flippass_typing_should_cancel(const App* app) {
    return app != NULL && app->typing_active && app->typing_cancel_requested;
}

bool flippass_typing_consume_pending_back(App* app) {
    furi_assert(app);

    if(app->typing_cancel_back_tick == 0U) {
        return false;
    }

    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    const uint32_t elapsed_ticks = furi_get_tick() - app->typing_cancel_back_tick;
    const uint32_t suppress_ticks =
        (tick_hz > 0U) ? ((tick_hz * FLIPPASS_TYPING_BACK_SUPPRESS_MS + 999U) / 1000U) : 1U;

    app->typing_cancel_back_tick = 0U;
    return elapsed_ticks <= suppress_ticks;
}

void flippass_log_reset(App* app) {
    UNUSED(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    flippass_secure_delete_file_with_storage(storage, FLIPPASS_LOG_FILE_PATH);

#if FLIPPASS_ENABLE_LOGS
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, FLIPPASS_LOG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(stream, "");
        file_stream_close(stream);
    }

    stream_free(stream);
#endif
    furi_record_close(RECORD_STORAGE);
}

void flippass_log_event(App* app, const char* format, ...) {
#if !FLIPPASS_ENABLE_LOGS
    UNUSED(app);
    UNUSED(format);
#else
    furi_assert(app);
    furi_assert(format);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(file_stream_open(stream, FLIPPASS_LOG_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        stream_write_cstring(stream, "AUTO: ");

        va_list args;
        va_start(args, format);
        stream_write_vaformat(stream, format, args);
        va_end(args);

        stream_write_cstring(stream, "\n");
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
#endif
}

#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE
static const char* flippass_heap_track_mode_name(FuriHalRtcHeapTrackMode mode) {
    switch(mode) {
    case FuriHalRtcHeapTrackModeNone:
        return "none";
    case FuriHalRtcHeapTrackModeMain:
        return "main";
    case FuriHalRtcHeapTrackModeTree:
        return "tree";
    case FuriHalRtcHeapTrackModeAll:
        return "all";
    default:
        return "unknown";
    }
}

static bool flippass_system_log_capture_flag_present(void) {
    bool present = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    present = storage_file_exists(storage, FLIPPASS_SYSTEM_LOG_ENABLE_FILE_PATH);
    furi_record_close(RECORD_STORAGE);
    return present;
}

static bool flippass_system_log_capture_requested(void) {
    return (furi_hal_rtc_get_heap_track_mode() != FuriHalRtcHeapTrackModeNone) ||
           ((furi_log_get_level() >= FuriLogLevelTrace) &&
            flippass_system_log_capture_flag_present());
}

static void flippass_system_log_reset_file(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    flippass_secure_delete_file_with_storage(storage, FLIPPASS_SYSTEM_LOG_FILE_PATH);

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, FLIPPASS_SYSTEM_LOG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(stream, "");
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static bool flippass_system_log_line_matches(const char* line) {
    if(line == NULL || line[0] == '\0') {
        return false;
    }

    return strstr(line, "[FlipPassGzip]") != NULL || strstr(line, "[FlipPassParser]") != NULL ||
           strstr(line, "[FlipPassVault]") != NULL ||
           strstr(line, "allocation balance:") != NULL ||
           strstr(line, "Stack watermark is") != NULL || strstr(line, "dangerously low") != NULL;
}

static void flippass_system_log_store_ring_line(App* app, const char* line, size_t line_len) {
    furi_assert(app);
    furi_assert(line);

    if(app->system_log_ring == NULL || line_len == 0U) {
        return;
    }

    const size_t slot_size = FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE;
    char* slot = app->system_log_ring + (app->system_log_ring_next * slot_size);
    const size_t copy_len = (line_len < (slot_size - 1U)) ? line_len : (slot_size - 1U);
    memcpy(slot, line, copy_len);
    slot[copy_len] = '\0';
    if(copy_len < line_len) {
        app->system_log_ring_dropped++;
    }

    app->system_log_ring_next = (app->system_log_ring_next + 1U) % FLIPPASS_SYSTEM_LOG_RING_LINES;
    if(app->system_log_ring_count < FLIPPASS_SYSTEM_LOG_RING_LINES) {
        app->system_log_ring_count++;
    }
}

static void flippass_system_log_append_line(App* app, const char* line) {
    furi_assert(app);
    furi_assert(line);

    if(app->system_log_capture_buffered && app->system_log_ring != NULL) {
        flippass_system_log_store_ring_line(app, line, strlen(line));
        return;
    }

    if(app->system_log_capture_bytes >= FLIPPASS_SYSTEM_LOG_MAX_BYTES) {
        return;
    }

    const size_t line_len = strlen(line);
    if(line_len == 0U) {
        return;
    }

    const size_t available = FLIPPASS_SYSTEM_LOG_MAX_BYTES - app->system_log_capture_bytes;
    if(available == 0U) {
        return;
    }

    const size_t copy_len = (line_len < available) ? line_len : available;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(file_stream_open(stream, FLIPPASS_SYSTEM_LOG_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        stream_write(stream, (const uint8_t*)line, copy_len);
        if(app->system_log_capture_bytes + copy_len < FLIPPASS_SYSTEM_LOG_MAX_BYTES) {
            stream_write_cstring(stream, "\n");
            app->system_log_capture_bytes += copy_len + 1U;
        } else {
            app->system_log_capture_bytes += copy_len;
        }
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static void flippass_system_log_flush_ring_to_file(App* app) {
    furi_assert(app);

    if(!app->system_log_capture_buffered || app->system_log_ring == NULL) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    flippass_secure_delete_file_with_storage(storage, FLIPPASS_SYSTEM_LOG_FILE_PATH);

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, FLIPPASS_SYSTEM_LOG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(size_t index = 0U; index < app->system_log_ring_count; index++) {
            const size_t ring_index =
                (app->system_log_ring_count == FLIPPASS_SYSTEM_LOG_RING_LINES) ?
                    ((app->system_log_ring_next + index) % FLIPPASS_SYSTEM_LOG_RING_LINES) :
                    index;
            const char* line =
                app->system_log_ring + (ring_index * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
            if(line[0] == '\0') {
                continue;
            }
            stream_write(stream, (const uint8_t*)line, strlen(line));
            stream_write_cstring(stream, "\n");
        }

        if(app->system_log_ring_dropped > 0U) {
            char tail[96];
            snprintf(
                tail,
                sizeof(tail),
                "ring dropped=%lu",
                (unsigned long)app->system_log_ring_dropped);
            stream_write(stream, (const uint8_t*)tail, strlen(tail));
            stream_write_cstring(stream, "\n");
        }

        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static void flippass_system_log_flush_line(App* app) {
    furi_assert(app);

    if(app->system_log_line_len == 0U) {
        return;
    }

    app->system_log_line[app->system_log_line_len] = '\0';
    if(flippass_system_log_line_matches(app->system_log_line)) {
        flippass_system_log_append_line(app, app->system_log_line);
    }

    app->system_log_line_len = 0U;
    app->system_log_line[0] = '\0';
}

static void flippass_system_log_handler(const uint8_t* data, size_t size, void* context) {
    App* app = context;
    if(app == NULL || !app->system_log_capture_enabled || app->system_log_capture_busy ||
       ((!app->system_log_capture_buffered) && flippass_system_log_capture_is_suspended()) ||
       data == NULL || size == 0U) {
        return;
    }

    app->system_log_capture_busy = true;

    for(size_t index = 0U; index < size; index++) {
        const char ch = (char)data[index];

        if(ch == '\r') {
            continue;
        }

        if(ch == '\n') {
            flippass_system_log_flush_line(app);
            continue;
        }

        if(app->system_log_line_len + 1U >= sizeof(app->system_log_line)) {
            flippass_system_log_flush_line(app);
        }

        if(app->system_log_line_len + 1U < sizeof(app->system_log_line)) {
            app->system_log_line[app->system_log_line_len++] = ch;
        }
    }

    app->system_log_capture_busy = false;
}

static void flippass_system_log_capture_init(App* app) {
    furi_assert(app);

    app->system_log_capture_enabled = false;
    app->system_log_capture_busy = false;
    app->system_log_capture_bytes = 0U;
    app->system_log_line_len = 0U;
    app->system_log_line[0] = '\0';
    app->system_log_ring = NULL;
    app->system_log_ring_count = 0U;
    app->system_log_ring_next = 0U;
    app->system_log_ring_dropped = 0U;
    app->system_log_capture_buffered = false;
    app->system_log_handler.callback = flippass_system_log_handler;
    app->system_log_handler.context = app;

    if(!flippass_system_log_capture_requested()) {
        return;
    }

    flippass_system_log_reset_file();
    app->system_log_ring =
        malloc(FLIPPASS_SYSTEM_LOG_RING_LINES * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
    if(app->system_log_ring != NULL) {
        memset(
            app->system_log_ring,
            0,
            FLIPPASS_SYSTEM_LOG_RING_LINES * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
        app->system_log_capture_buffered = true;
    }
    app->system_log_capture_enabled = furi_log_add_handler(app->system_log_handler);

    if(app->system_log_capture_enabled) {
        char header[160];
        snprintf(
            header,
            sizeof(header),
            "capture enabled level=%d heap=%s",
            (int)furi_log_get_level(),
            flippass_heap_track_mode_name(furi_hal_rtc_get_heap_track_mode()));
        flippass_system_log_append_line(app, header);
        FLIPPASS_LOG_EVENT(
            app,
            "SYSTEM_TRACE_CAPTURE level=%d heap=%s",
            (int)furi_log_get_level(),
            flippass_heap_track_mode_name(furi_hal_rtc_get_heap_track_mode()));
    }
}

static void flippass_system_log_capture_deinit(App* app) {
    furi_assert(app);

    if(!app->system_log_capture_enabled) {
        return;
    }

    furi_log_remove_handler(app->system_log_handler);
    app->system_log_capture_enabled = false;
    flippass_system_log_flush_line(app);
    flippass_system_log_flush_ring_to_file(app);
    app->system_log_capture_busy = false;
    if(app->system_log_ring != NULL) {
        memzero(
            app->system_log_ring,
            FLIPPASS_SYSTEM_LOG_RING_LINES * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
        free(app->system_log_ring);
        app->system_log_ring = NULL;
    }
    app->system_log_ring_count = 0U;
    app->system_log_ring_next = 0U;
    app->system_log_ring_dropped = 0U;
    app->system_log_capture_buffered = false;
}

void flippass_system_log_capture_suspend(void) {
    flippass_system_log_capture_suspend_depth++;
}

void flippass_system_log_capture_resume(void) {
    if(flippass_system_log_capture_suspend_depth > 0U) {
        flippass_system_log_capture_suspend_depth--;
    }
}

bool flippass_system_log_capture_is_suspended(void) {
    return flippass_system_log_capture_suspend_depth > 0U;
}
#else
static void flippass_system_log_capture_init(App* app) {
    UNUSED(app);
}

static void flippass_system_log_capture_deinit(App* app) {
    UNUSED(app);
}

void flippass_system_log_capture_suspend(void) {
}

void flippass_system_log_capture_resume(void) {
}

bool flippass_system_log_capture_is_suspended(void) {
    return false;
}
#endif

static bool flippass_keyboard_layout_is_valid(const char* path) {
    bool valid = false;

    if(path == NULL || path[0] == '\0' || strcmp(path, FLIPPASS_KEYBOARD_LAYOUT_ALT) == 0) {
        return true;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo file_info;
    if(storage_common_stat(storage, path, &file_info) == FSE_OK &&
       !(file_info.flags & FSF_DIRECTORY) && file_info.size == 256U) {
        valid = true;
    }
    furi_record_close(RECORD_STORAGE);

    return valid;
}

void flippass_clear_text_buffer(App* app) {
    furi_assert(app);
    memzero(app->text_buffer, sizeof(app->text_buffer));
}

void flippass_clear_master_password(App* app) {
    furi_assert(app);
    memzero(app->master_password, sizeof(app->master_password));
}

void flippass_set_status(App* app, const char* title, const char* message) {
    furi_assert(app);

    snprintf(app->status_title, sizeof(app->status_title), "%s", title ? title : "Status");
    snprintf(app->status_message, sizeof(app->status_message), "%s", message ? message : "");
}

void flippass_progress_reset(App* app) {
    furi_assert(app);

    app->progress_started_tick = 0U;
    app->progress_percent = 0U;
    app->progress_title[0] = '\0';
    if(app->progress_view != NULL) {
        flippass_progress_view_reset(app->progress_view);
    }
}

void flippass_progress_begin(App* app, const char* title, const char* stage, uint8_t percent) {
    furi_assert(app);

    snprintf(
        app->progress_title, sizeof(app->progress_title), "%s", title != NULL ? title : "Working");
    app->progress_started_tick = furi_get_tick();
    app->progress_percent = 0U;
    flippass_progress_update(app, stage, "", percent);
}

void flippass_progress_update(App* app, const char* stage, const char* detail, uint8_t percent) {
    char detail_buffer[STATUS_MESSAGE_SIZE];
    char eta_text[24];
    const uint8_t clamped = (percent <= 100U) ? percent : 100U;
    const char* detail_text = detail != NULL ? detail : "";

    furi_assert(app);

    detail_buffer[0] = '\0';
    eta_text[0] = '\0';
    if(clamped >= 5U && clamped < 100U && app->progress_started_tick != 0U) {
        const uint32_t tick_delta = furi_get_tick() - app->progress_started_tick;
        const uint32_t tick_hz = furi_kernel_get_tick_frequency();
        if(tick_hz > 0U) {
            const uint64_t elapsed_ms = ((uint64_t)tick_delta * 1000U) / tick_hz;
            const uint64_t remaining_ms = (elapsed_ms * (100U - clamped)) / clamped;
            const uint32_t remaining_sec = (uint32_t)((remaining_ms + 999U) / 1000U);
            if(remaining_sec >= 60U) {
                snprintf(
                    eta_text,
                    sizeof(eta_text),
                    "~%lum %lus left",
                    (unsigned long)(remaining_sec / 60U),
                    (unsigned long)(remaining_sec % 60U));
            } else {
                snprintf(eta_text, sizeof(eta_text), "~%lus left", (unsigned long)remaining_sec);
            }
        }
    }

    if(detail_text[0] != '\0' && eta_text[0] != '\0') {
        snprintf(detail_buffer, sizeof(detail_buffer), "%s  %s", detail_text, eta_text);
    } else if(detail_text[0] != '\0') {
        snprintf(detail_buffer, sizeof(detail_buffer), "%s", detail_text);
    } else if(eta_text[0] != '\0') {
        snprintf(detail_buffer, sizeof(detail_buffer), "%s", eta_text);
    }

    app->progress_percent = clamped;
    if(app->progress_view != NULL) {
        flippass_progress_view_set_state(
            app->progress_view,
            app->progress_title[0] != '\0' ? app->progress_title : "Working",
            stage != NULL ? stage : "",
            detail_buffer,
            clamped);
    }
}

void flippass_reset_database(App* app) {
    furi_assert(app);

    if(app->root_group != NULL || app->vault != NULL || app->db_arena != NULL ||
       app->database_loaded) {
        FLIPPASS_DIAGNOSTIC_LOG(app, "VAULT_CLEANUP");
    }

    flippass_db_deactivate_entry(app);
    kdbx_group_free(app->root_group);
    if(app->vault != NULL) {
        kdbx_vault_free(app->vault);
    }
    if(app->db_arena != NULL) {
        kdbx_arena_free(app->db_arena);
    }
    if(app->pending_gzip_scratch_vault != NULL) {
        kdbx_vault_free(app->pending_gzip_scratch_vault);
    }
    app->db_arena = NULL;
    app->vault = NULL;
    app->active_vault_backend = KDBXVaultBackendNone;
    app->requested_vault_backend = KDBXVaultBackendRam;
    app->pending_gzip_scratch_vault = NULL;
    memset(&app->pending_gzip_scratch_ref, 0, sizeof(app->pending_gzip_scratch_ref));
    app->pending_gzip_plain_size = 0U;
    app->root_group = NULL;
    app->current_group = NULL;
    app->current_entry = NULL;
    app->active_group = NULL;
    app->active_entry = NULL;
    app->browser_selected_index = 0;
    app->action_selected_index = 0;
    app->other_field_selected_index = 0;
    app->other_field_action_selected_index = 0;
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_field_name[0] = '\0';
    app->close_db_dialog_open = false;
    app->parse_failed = false;
    app->database_loaded = false;
    app->pending_vault_fallback = false;
    app->allow_ext_vault_promotion = false;
    if(app->text_view_body != NULL) {
        furi_string_reset(app->text_view_body);
    }
    app->text_view_title[0] = '\0';
    app->text_view_return_scene = FlipPassScene_DbEntries;
}

void flippass_close_database(App* app) {
    furi_assert(app);

    FLIPPASS_DIAGNOSTIC_LOG(app, "DATABASE_CLOSE");
    flippass_usb_restore(app);
    flippass_output_release_all(app);
    dialog_ex_reset(app->dialog_ex);
    widget_reset(app->widget);
    submenu_reset(app->submenu);
    text_input_reset(app->text_input);
    flippass_db_browser_view_reset(app->db_browser);
    flippass_progress_reset(app);
    flippass_reset_database(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    app->password_header[0] = '\0';
    flippass_set_status(app, "FlipPass", "");
    app->status_return_scene = FlipPassScene_FileBrowser;
}

void flippass_save_settings(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    flippass_secure_delete_file_with_storage(storage, FLIPPASS_CONFIG_FILE_PATH);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(file, FLIPPASS_CONFIG_FILE_PATH)) {
        flipper_format_write_string_cstr(
            file,
            "keyboard_layout",
            (app->keyboard_layout_path != NULL && !furi_string_empty(app->keyboard_layout_path)) ?
                furi_string_get_cstr(app->keyboard_layout_path) :
                FLIPPASS_KEYBOARD_LAYOUT_ALT);
        flippass_secure_write_encrypted_string(
            file, "last_file_path", furi_string_get_cstr(app->file_path));
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void flippass_load_settings(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* keyboard_layout = furi_string_alloc();
    FuriString* legacy_last_file_path = furi_string_alloc();
    bool rewrite_settings = false;

    if(flipper_format_file_open_existing(file, FLIPPASS_CONFIG_FILE_PATH)) {
        flipper_format_rewind(file);
        rewrite_settings =
            flipper_format_read_string(file, "last_file_path", legacy_last_file_path);
        flipper_format_rewind(file);
        if(!flippass_secure_read_encrypted_string(file, "last_file_path", app->file_path)) {
            furi_string_reset(app->file_path);
        }
        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "keyboard_layout", keyboard_layout) &&
           !furi_string_empty(keyboard_layout)) {
            const char* layout_path = furi_string_get_cstr(keyboard_layout);
            if(strcmp(layout_path, FLIPPASS_KEYBOARD_LAYOUT_ALT) != 0 &&
               flippass_keyboard_layout_is_valid(layout_path)) {
                furi_string_set(app->keyboard_layout_path, keyboard_layout);
            } else {
                furi_string_reset(app->keyboard_layout_path);
            }
        } else {
            furi_string_reset(app->keyboard_layout_path);
        }
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(keyboard_layout);
    furi_string_free(legacy_last_file_path);

    if(rewrite_settings) {
        flippass_save_settings(app);
    }
}

/**
 * @brief Custom event callback for the scene manager.
 *
 * This function is called when a custom event is triggered in the application.
 * It passes the event to the scene manager for handling.
 *
 * @param context The application context.
 * @param event The custom event that was triggered.
 * @return True if the event was handled, false otherwise.
 */

/**
 * @brief Allocates and initializes the application.
 *
 * This function allocates the main application struct and initializes all its
 * components, including the GUI, view dispatcher, scene manager, and views.
 *
 * @return A pointer to the allocated App struct.
 */
static App* flippass_app_alloc(const char* args) {
    App* app = malloc(sizeof(App));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    app->scene_manager = scene_manager_alloc(&flippass_scene_handlers, app);
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, flippass_input_events_callback, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flippass_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flippass_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->file_path = furi_string_alloc();
    app->keyboard_layout_path = furi_string_alloc();
    app->file_browser = file_browser_alloc(app->file_path);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewFileBrowser, file_browser_get_view(app->file_browser));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewPasswordEntry, text_input_get_view(app->text_input));

    app->progress_view = flippass_progress_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewLoading, flippass_progress_view_get_view(app->progress_view));

    app->db_browser = flippass_db_browser_view_alloc();
    flippass_db_browser_view_set_back_filter(app->db_browser, flippass_db_browser_back_filter);
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewDbBrowser,
        flippass_db_browser_view_get_view(app->db_browser));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewWidget, widget_get_view(app->widget));

    app->dialog_ex = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->db_arena = NULL;
    app->vault = NULL;
    app->active_vault_backend = KDBXVaultBackendNone;
    app->requested_vault_backend = KDBXVaultBackendRam;
    app->pending_gzip_scratch_vault = NULL;
    memset(&app->pending_gzip_scratch_ref, 0, sizeof(app->pending_gzip_scratch_ref));
    app->pending_gzip_plain_size = 0U;
    app->root_group = NULL;
    app->current_group = NULL;
    app->current_entry = NULL;
    app->active_group = NULL;
    app->active_entry = NULL;
    app->text_view_body = furi_string_alloc();
    app->usb_expect_rpc_session_close = false;
    app->browser_selected_index = 0;
    app->action_selected_index = 0;
    app->other_field_selected_index = 0;
    app->other_field_action_selected_index = 0;
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_field_name[0] = '\0';
    app->keyboard_layout_return_scene = FlipPassScene_DbEntries;
    app->close_db_dialog_open = false;
    app->parse_failed = false;
    app->database_loaded = false;
    app->pending_vault_fallback = false;
    app->allow_ext_vault_promotion = false;
    app->close_test_logged = false;
    app->status_return_scene = FlipPassScene_FileBrowser;
    app->text_view_title[0] = '\0';
    app->text_view_return_scene = FlipPassScene_DbEntries;
    app->progress_started_tick = 0U;
    app->progress_percent = 0U;
    app->progress_title[0] = '\0';
    app->rpc = NULL;
    app->rpc_mode = false;
    app->typing_active = false;
    app->typing_cancel_requested = false;
    app->typing_cancel_back_tick = 0U;
    flippass_module_loader_init(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    flippass_set_status(app, "FlipPass", "");
    flippass_log_reset(app);
    flippass_system_log_capture_init(app);
    FLIPPASS_LOG_EVENT(app, "APP_START");
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=session_cleanup_begin");
    Storage* storage_for_cleanup = furi_record_open(RECORD_STORAGE);
    kdbx_vault_cleanup_runtime_sessions(storage_for_cleanup);
    furi_record_close(RECORD_STORAGE);
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=session_cleanup_done");
    flippass_rpc_init(app, args);
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=rpc_init mode=%u", app->rpc_mode ? 1U : 0U);

    flippass_load_settings(app);
    FLIPPASS_STARTUP_LOG(
        app,
        "APP_INIT_STEP step=load_settings has_path=%u",
        (app->file_path != NULL && !furi_string_empty(app->file_path)) ? 1U : 0U);

    // Start the application on the file browser scene
    scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=file_browser_scene");

    // If a valid last file is found, transition to the password entry scene
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool has_last_file = app->file_path != NULL &&
                               storage_file_exists(storage, furi_string_get_cstr(app->file_path));
    FLIPPASS_STARTUP_LOG(
        app, "APP_INIT_STEP step=last_file_exists ok=%u", has_last_file ? 1U : 0U);
    if(has_last_file) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
        FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=password_entry_scene");
    }
    furi_record_close(RECORD_STORAGE);

    return app;
}

/**
 * @brief Frees the application and its resources.
 *
 * This function deallocates all the resources used by the application,
 * including views, view dispatcher, scene manager, and the app struct itself.
 *
 * @param app A pointer to the App struct to free.
 */
static void flippass_app_free(App* app) {
    furi_assert(app);

    furi_pubsub_unsubscribe(app->input_events, app->input_subscription);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewPasswordEntry);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewDbBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewDialogEx);
    dialog_ex_free(app->dialog_ex);
    widget_free(app->widget);
    submenu_free(app->submenu);
    flippass_db_browser_view_free(app->db_browser);
    flippass_progress_view_free(app->progress_view);
    text_input_free(app->text_input);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_path);
    furi_string_free(app->keyboard_layout_path);
    flippass_reset_database(app);
    flippass_output_cleanup(app);
    furi_string_free(app->text_view_body);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    FLIPPASS_LOG_EVENT(app, "APP_EXIT");
    flippass_system_log_capture_deinit(app);
    flippass_rpc_deinit(app);
    flippass_module_loader_deinit(app);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);
    free(app);
}

/**
 * @brief Main entry point for the FlipPass application.
 *
 * This function is the main entry point for the application. It allocates
 * the app, runs the view dispatcher, and then frees the app resources.
 *
 * @param p Unused parameter.
 * @return 0 on success.
 */
int32_t flippass_app(void* p) {
    const char* args = p;
    App* app = flippass_app_alloc(args);
    view_dispatcher_run(app->view_dispatcher);
    flippass_app_free(app);
    return 0;
}
