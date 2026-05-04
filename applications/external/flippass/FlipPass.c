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
#include "kdbx/kdbx_constants.h"
#include "scenes/flippass_db_browser_view.h"
#include "scenes/flippass_progress_view.h"
#include "scenes/flippass_scene_editor.h"
#include "scenes/flippass_scene_password_generator.h"
#include "scenes/flippass_scene_status.h"
#include <flipper_format/flipper_format.h>
#include <input/input.h>
#include <stdio.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TAG                                         "FlipPass"
#define FLIPPASS_SYSTEM_LOG_MAX_BYTES               (24U * 1024U)
#define FLIPPASS_TYPING_BACK_SUPPRESS_MS            250U
#define FLIPPASS_SCENE_TICK_MS                      100U
#define FLIPPASS_SECURE_STORE_SEED                  "FlipPass sealed storage v1"
#define FLIPPASS_SESSION_CREDENTIAL_SEED            "FlipPass session credential v1"
#define FLIPPASS_SESSION_SAVE_KEY_LABEL             "database_save_key"
#define FLIPPASS_SESSION_SAVE_TRANSFORMED_KEY_LABEL "database_save_transformed_key"
#define FLIPPASS_BADUSB_SETTINGS_FILE_TYPE          "Flipper BadUSB Settings File"
#define FLIPPASS_BADUSB_SETTINGS_VERSION            1U

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

typedef enum {
    FlipPassSecureKeyModeSeedOnly = 0,
    FlipPassSecureKeyModeDeviceUnique,
} FlipPassSecureKeyMode;

static bool
    flippass_keyboard_layout_file_is_valid_with_storage(Storage* storage, const char* path);
static bool flippass_load_badusb_keyboard_layout(Storage* storage, FuriString* out_path);
#if FLIPPASS_ENABLE_DEBUG_CREATE_HOOK
static bool flippass_try_debug_create(App* app);
#endif
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE
static void flippass_system_log_capture_init(App* app);
static void flippass_system_log_capture_deinit(App* app);
#else
static void flippass_system_log_capture_init(App* app);
static void flippass_system_log_capture_deinit(App* app);
#endif
static void flippass_update_last_interaction(App* app);
static void flippass_idle_tick(App* app);
static bool flippass_secure_write_device_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    const char* value);
static bool flippass_secure_read_device_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    FuriString* out_value,
    bool allow_legacy_fallback,
    bool* out_used_legacy);

static void flippass_input_events_callback(const void* message, void* context) {
    const InputEvent* event = message;
    App* app = context;

    if(event == NULL || app == NULL) {
        return;
    }

    flippass_update_last_interaction(app);
    flippass_password_generator_input_event(app, event);

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

static void flippass_tick_event_callback(void* context) {
    App* app = context;

    if(app != NULL && app->scene_manager != NULL) {
        flippass_idle_tick(app);
        scene_manager_handle_tick_event(app->scene_manager);
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

static void flippass_update_last_interaction(App* app) {
    furi_assert(app);

    furi_hal_rtc_get_datetime(&app->last_interaction_datetime);
    app->last_interaction_tick = furi_get_tick();
}

static void flippass_secure_storage_derive_seed_keys(
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

static bool flippass_secure_storage_derive_device_keys(
    const char* key_prefix,
    bool create_if_missing,
    uint8_t enc_key[32],
    uint8_t mac_key[32]) {
    uint8_t material[64];
    uint8_t encrypted_material[64];
    uint8_t iv[16];
    SHA512_CTX ctx;
    bool ok = false;
    bool loaded = false;

    furi_assert(key_prefix);
    furi_assert(enc_key);
    furi_assert(mac_key);

    memset(iv, 0, sizeof(iv));
    memset(encrypted_material, 0, sizeof(encrypted_material));

    sha512_Init(&ctx);
    sha512_Update(
        &ctx, (const uint8_t*)FLIPPASS_SECURE_STORE_SEED, strlen(FLIPPASS_SECURE_STORE_SEED));
    sha512_Update(&ctx, (const uint8_t*)key_prefix, strlen(key_prefix));
    sha512_Final(&ctx, material);

    if(create_if_missing &&
       !furi_hal_crypto_enclave_ensure_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
        goto cleanup;
    }

    if(!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        goto cleanup;
    }
    loaded = true;

    if(!furi_hal_crypto_encrypt(material, encrypted_material, sizeof(encrypted_material))) {
        goto cleanup;
    }

    loaded = false;
    if(!furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
        goto cleanup;
    }

    memcpy(enc_key, encrypted_material, 32U);
    memcpy(mac_key, encrypted_material + 32U, 32U);
    ok = true;

cleanup:
    if(loaded) {
        const bool unload_ok =
            furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
        ok = ok && unload_ok;
    }
    memzero(material, sizeof(material));
    memzero(encrypted_material, sizeof(encrypted_material));
    memzero(iv, sizeof(iv));
    return ok;
}

static bool flippass_secure_storage_derive_keys(
    FlipPassSecureKeyMode mode,
    const char* key_prefix,
    bool create_if_missing,
    uint8_t enc_key[32],
    uint8_t mac_key[32]) {
    switch(mode) {
    case FlipPassSecureKeyModeSeedOnly:
        flippass_secure_storage_derive_seed_keys(key_prefix, enc_key, mac_key);
        return true;
    case FlipPassSecureKeyModeDeviceUnique:
        return flippass_secure_storage_derive_device_keys(
            key_prefix, create_if_missing, enc_key, mac_key);
    default:
        return false;
    }
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

static bool flippass_constant_time_equal(const uint8_t* left, const uint8_t* right, size_t size) {
    uint8_t diff = 0U;

    furi_assert(left);
    furi_assert(right);

    for(size_t index = 0U; index < size; index++) {
        diff |= left[index] ^ right[index];
    }

    return diff == 0U;
}

static uint32_t flippass_ms_to_ticks(uint32_t ms) {
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    if(tick_hz == 0U) {
        return ms;
    }

    const uint64_t ticks = (((uint64_t)ms * tick_hz) + 999U) / 1000U;
    return ticks > UINT32_MAX ? UINT32_MAX : (uint32_t)ticks;
}

static uint32_t flippass_minutes_to_ms(uint16_t minutes) {
    return (uint32_t)minutes * 60U * 1000U;
}

void flippass_make_password_composite_key(const char* password, uint8_t out_key[32]) {
    uint8_t password_hash[32];

    furi_assert(password);
    furi_assert(out_key);

    sha256_Raw((const uint8_t*)password, strlen(password), password_hash);
    sha256_Raw(password_hash, sizeof(password_hash), out_key);
    memzero(password_hash, sizeof(password_hash));
}

static void flippass_session_derive_keys(
    const uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE],
    const char* purpose,
    uint8_t enc_key[32],
    uint8_t mac_key[32]) {
    uint8_t hash[64];
    SHA512_CTX ctx;

    furi_assert(session_key);
    furi_assert(purpose);
    furi_assert(enc_key);
    furi_assert(mac_key);

    sha512_Init(&ctx);
    sha512_Update(
        &ctx,
        (const uint8_t*)FLIPPASS_SESSION_CREDENTIAL_SEED,
        strlen(FLIPPASS_SESSION_CREDENTIAL_SEED));
    sha512_Update(&ctx, (const uint8_t*)purpose, strlen(purpose));
    sha512_Update(&ctx, session_key, FLIPPASS_SESSION_SECRET_SIZE);
    sha512_Final(&ctx, hash);
    memcpy(enc_key, hash, 32U);
    memcpy(mac_key, hash + 32U, 32U);
    memzero(hash, sizeof(hash));
}

static bool flippass_session_wrap_key(
    const uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE],
    uint8_t iv[FLIPPASS_SESSION_WRAP_IV_SIZE],
    uint8_t wrapped_key[FLIPPASS_SESSION_SECRET_SIZE]) {
    bool ok = false;
    bool loaded = false;

    furi_assert(session_key);
    furi_assert(iv);
    furi_assert(wrapped_key);

    furi_hal_random_fill_buf(iv, FLIPPASS_SESSION_WRAP_IV_SIZE);
    if(!furi_hal_crypto_enclave_ensure_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT)) {
        goto cleanup;
    }

    if(!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        goto cleanup;
    }
    loaded = true;

    ok = furi_hal_crypto_encrypt(session_key, wrapped_key, FLIPPASS_SESSION_SECRET_SIZE);

cleanup:
    if(loaded) {
        const bool unload_ok =
            furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
        ok = ok && unload_ok;
    }
    if(!ok) {
        memzero(iv, FLIPPASS_SESSION_WRAP_IV_SIZE);
        memzero(wrapped_key, FLIPPASS_SESSION_SECRET_SIZE);
    }
    return ok;
}

static bool flippass_session_unwrap_key(
    const uint8_t iv[FLIPPASS_SESSION_WRAP_IV_SIZE],
    const uint8_t wrapped_key[FLIPPASS_SESSION_SECRET_SIZE],
    uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE]) {
    bool ok = false;
    bool loaded = false;

    furi_assert(iv);
    furi_assert(wrapped_key);
    furi_assert(session_key);

    if(!furi_hal_crypto_enclave_load_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT, iv)) {
        goto cleanup;
    }
    loaded = true;

    ok = furi_hal_crypto_decrypt(wrapped_key, session_key, FLIPPASS_SESSION_SECRET_SIZE);

cleanup:
    if(loaded) {
        const bool unload_ok =
            furi_hal_crypto_enclave_unload_key(FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT);
        ok = ok && unload_ok;
    }
    if(!ok) {
        memzero(session_key, FLIPPASS_SESSION_SECRET_SIZE);
    }
    return ok;
}

static void flippass_session_seal_key32(
    const uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE],
    const char* label,
    const uint8_t value[32],
    uint8_t nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE],
    uint8_t cipher[32],
    uint8_t mac[FLIPPASS_SECURE_VALUE_MAC_SIZE]) {
    uint8_t enc_key[32];
    uint8_t mac_key[32];

    furi_assert(session_key);
    furi_assert(label);
    furi_assert(value);
    furi_assert(nonce);
    furi_assert(cipher);
    furi_assert(mac);

    furi_hal_random_fill_buf(nonce, FLIPPASS_SECURE_VALUE_NONCE_SIZE);
    flippass_session_derive_keys(session_key, label, enc_key, mac_key);
    memcpy(cipher, value, 32U);
    flippass_secure_storage_xor(cipher, 32U, enc_key, nonce);
    flippass_secure_storage_mac(label, mac_key, nonce, cipher, 32U, mac);

    memzero(enc_key, sizeof(enc_key));
    memzero(mac_key, sizeof(mac_key));
}

static bool flippass_session_unseal_key32(
    const uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE],
    const char* label,
    const uint8_t nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE],
    const uint8_t cipher[32],
    const uint8_t mac[FLIPPASS_SECURE_VALUE_MAC_SIZE],
    uint8_t out_key[32]) {
    uint8_t enc_key[32];
    uint8_t mac_key[32];
    uint8_t expected_mac[FLIPPASS_SECURE_VALUE_MAC_SIZE];
    bool ok = false;

    furi_assert(session_key);
    furi_assert(label);
    furi_assert(nonce);
    furi_assert(cipher);
    furi_assert(mac);
    furi_assert(out_key);

    flippass_session_derive_keys(session_key, label, enc_key, mac_key);
    flippass_secure_storage_mac(label, mac_key, nonce, cipher, 32U, expected_mac);
    if(!flippass_constant_time_equal(mac, expected_mac, sizeof(expected_mac))) {
        goto cleanup;
    }

    memcpy(out_key, cipher, 32U);
    flippass_secure_storage_xor(out_key, 32U, enc_key, nonce);
    ok = true;

cleanup:
    if(!ok) {
        memzero(out_key, 32U);
    }
    memzero(enc_key, sizeof(enc_key));
    memzero(mac_key, sizeof(mac_key));
    memzero(expected_mac, sizeof(expected_mac));
    return ok;
}

void flippass_session_clear_credentials(App* app) {
    furi_assert(app);

    memzero(app->session_key_iv, sizeof(app->session_key_iv));
    memzero(app->session_key_cipher, sizeof(app->session_key_cipher));
    memzero(app->database_save_key_nonce, sizeof(app->database_save_key_nonce));
    memzero(app->database_save_key_cipher, sizeof(app->database_save_key_cipher));
    memzero(app->database_save_key_mac, sizeof(app->database_save_key_mac));
    memzero(
        app->database_save_transformed_key_nonce,
        sizeof(app->database_save_transformed_key_nonce));
    memzero(
        app->database_save_transformed_key_cipher,
        sizeof(app->database_save_transformed_key_cipher));
    memzero(
        app->database_save_transformed_key_mac, sizeof(app->database_save_transformed_key_mac));
    memzero(app->database_save_kdf_salt, sizeof(app->database_save_kdf_salt));
    app->database_save_kdf_rounds = 0U;
    app->database_save_key_ready = false;
    app->database_save_transformed_key_ready = false;
}

bool flippass_session_store_save_material(
    App* app,
    const uint8_t save_key[32],
    const uint8_t* transformed_key,
    const uint8_t* kdf_salt,
    uint64_t kdf_rounds) {
    uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE];
    uint8_t wrapped_key[FLIPPASS_SESSION_SECRET_SIZE];
    uint8_t wrap_iv[FLIPPASS_SESSION_WRAP_IV_SIZE];
    bool ok = false;
    const bool has_transformed = transformed_key != NULL && kdf_salt != NULL && kdf_rounds != 0U;

    furi_assert(app);
    furi_assert(save_key);

    flippass_session_clear_credentials(app);

    furi_hal_random_fill_buf(session_key, sizeof(session_key));
    if(!flippass_session_wrap_key(session_key, wrap_iv, wrapped_key)) {
        goto cleanup;
    }

    flippass_session_seal_key32(
        session_key,
        FLIPPASS_SESSION_SAVE_KEY_LABEL,
        save_key,
        app->database_save_key_nonce,
        app->database_save_key_cipher,
        app->database_save_key_mac);

    if(has_transformed) {
        flippass_session_seal_key32(
            session_key,
            FLIPPASS_SESSION_SAVE_TRANSFORMED_KEY_LABEL,
            transformed_key,
            app->database_save_transformed_key_nonce,
            app->database_save_transformed_key_cipher,
            app->database_save_transformed_key_mac);
        memcpy(app->database_save_kdf_salt, kdf_salt, sizeof(app->database_save_kdf_salt));
        app->database_save_kdf_rounds = kdf_rounds;
        app->database_save_transformed_key_ready = true;
    }

    memcpy(app->session_key_iv, wrap_iv, sizeof(app->session_key_iv));
    memcpy(app->session_key_cipher, wrapped_key, sizeof(app->session_key_cipher));
    app->database_save_key_ready = true;
    ok = true;

cleanup:
    if(!ok) {
        flippass_session_clear_credentials(app);
    }
    memzero(session_key, sizeof(session_key));
    memzero(wrapped_key, sizeof(wrapped_key));
    memzero(wrap_iv, sizeof(wrap_iv));
    return ok;
}

bool flippass_session_store_save_key(App* app, const uint8_t save_key[32]) {
    return flippass_session_store_save_material(app, save_key, NULL, NULL, 0U);
}

bool flippass_session_copy_save_key(App* app, uint8_t out_key[32]) {
    uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE];
    bool ok = false;

    furi_assert(app);
    furi_assert(out_key);

    memzero(out_key, 32U);
    if(!app->database_save_key_ready) {
        return false;
    }

    if(!flippass_session_unwrap_key(app->session_key_iv, app->session_key_cipher, session_key)) {
        goto cleanup;
    }

    ok = flippass_session_unseal_key32(
        session_key,
        FLIPPASS_SESSION_SAVE_KEY_LABEL,
        app->database_save_key_nonce,
        app->database_save_key_cipher,
        app->database_save_key_mac,
        out_key);

cleanup:
    if(!ok) {
        memzero(out_key, 32U);
    }
    memzero(session_key, sizeof(session_key));
    return ok;
}

bool flippass_session_copy_save_transformed_key(
    App* app,
    uint8_t out_key[32],
    uint8_t out_kdf_salt[32],
    uint64_t* out_kdf_rounds) {
    uint8_t session_key[FLIPPASS_SESSION_SECRET_SIZE];
    bool ok = false;

    furi_assert(app);
    furi_assert(out_key);
    furi_assert(out_kdf_salt);
    furi_assert(out_kdf_rounds);

    memzero(out_key, 32U);
    memzero(out_kdf_salt, 32U);
    *out_kdf_rounds = 0U;
    if(!app->database_save_transformed_key_ready || app->database_save_kdf_rounds == 0U) {
        return false;
    }

    if(!flippass_session_unwrap_key(app->session_key_iv, app->session_key_cipher, session_key)) {
        goto cleanup;
    }

    if(!flippass_session_unseal_key32(
           session_key,
           FLIPPASS_SESSION_SAVE_TRANSFORMED_KEY_LABEL,
           app->database_save_transformed_key_nonce,
           app->database_save_transformed_key_cipher,
           app->database_save_transformed_key_mac,
           out_key)) {
        goto cleanup;
    }

    memcpy(out_kdf_salt, app->database_save_kdf_salt, 32U);
    *out_kdf_rounds = app->database_save_kdf_rounds;
    ok = true;

cleanup:
    if(!ok) {
        memzero(out_key, 32U);
        memzero(out_kdf_salt, 32U);
        *out_kdf_rounds = 0U;
    }
    memzero(session_key, sizeof(session_key));
    return ok;
}

bool flippass_session_verify_password(App* app, const char* password) {
    uint8_t expected_key[32];
    uint8_t stored_key[32];
    bool ok = false;

    furi_assert(app);

    if(password == NULL || password[0] == '\0') {
        return false;
    }

    flippass_make_password_composite_key(password, expected_key);
    ok = flippass_session_copy_save_key(app, stored_key) &&
         flippass_constant_time_equal(expected_key, stored_key, sizeof(expected_key));

    memzero(expected_key, sizeof(expected_key));
    memzero(stored_key, sizeof(stored_key));
    return ok;
}

static bool flippass_secure_write_encrypted_string_mode(
    FlipperFormat* file,
    const char* key_prefix,
    const char* value,
    FlipPassSecureKeyMode mode) {
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
    if(!flippass_secure_storage_derive_keys(mode, key_prefix, true, enc_key, mac_key)) {
        goto cleanup;
    }
    flippass_secure_storage_xor(ciphertext, value_len, enc_key, nonce);
    flippass_secure_storage_mac(key_prefix, mac_key, nonce, ciphertext, value_len, mac);

    ok = flipper_format_write_hex(file, nonce_key, nonce, sizeof(nonce)) &&
         flipper_format_write_hex(file, cipher_key, ciphertext, (uint16_t)value_len) &&
         flipper_format_write_hex(file, mac_field_key, mac, sizeof(mac));

cleanup:
    memzero(enc_key, sizeof(enc_key));
    memzero(mac_key, sizeof(mac_key));
    memzero(nonce, sizeof(nonce));
    memzero(mac, sizeof(mac));
    if(ciphertext != NULL) {
        memzero(ciphertext, value_len);
        free(ciphertext);
    }
    return ok;
}

static bool flippass_secure_read_encrypted_string_mode(
    FlipperFormat* file,
    const char* key_prefix,
    FuriString* out_value,
    FlipPassSecureKeyMode mode) {
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

    if(!flippass_secure_storage_derive_keys(mode, key_prefix, false, enc_key, mac_key)) {
        goto cleanup;
    }
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
    return flippass_secure_write_encrypted_string_mode(
        file, key_prefix, value, FlipPassSecureKeyModeSeedOnly);
}

bool flippass_secure_read_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    FuriString* out_value) {
    return flippass_secure_read_encrypted_string_mode(
        file, key_prefix, out_value, FlipPassSecureKeyModeSeedOnly);
}

static bool flippass_secure_write_device_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    const char* value) {
    return flippass_secure_write_encrypted_string_mode(
        file, key_prefix, value, FlipPassSecureKeyModeDeviceUnique);
}

static bool flippass_secure_read_device_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    FuriString* out_value,
    bool allow_legacy_fallback,
    bool* out_used_legacy) {
    if(out_used_legacy != NULL) {
        *out_used_legacy = false;
    }

    if(flippass_secure_read_encrypted_string_mode(
           file, key_prefix, out_value, FlipPassSecureKeyModeDeviceUnique)) {
        return true;
    }

    if(!allow_legacy_fallback) {
        furi_string_reset(out_value);
        return false;
    }

    if(flippass_secure_read_encrypted_string(file, key_prefix, out_value)) {
        if(out_used_legacy != NULL) {
            *out_used_legacy = true;
        }
        return true;
    }

    furi_string_reset(out_value);
    return false;
}

static bool flippass_idle_lock_can_prompt(const App* app, uint32_t current_scene) {
    if(app == NULL || app->idle_lock_active || app->typing_active ||
       app->progress_started_tick != 0U) {
        return false;
    }

    if(!app->database_loaded || app->root_group == NULL || !app->database_save_key_ready) {
        return false;
    }

    return current_scene != FlipPassScene_PasswordEntry &&
           current_scene != FlipPassScene_VaultFallback &&
           current_scene != FlipPassScene_PasswordGeneratorHarvest;
}

static void flippass_idle_request_lock(App* app) {
    furi_assert(app);

    app->idle_lock_active = true;
    app->idle_lock_failed_attempts = 0U;
    flippass_output_cleanup(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    FLIPPASS_LOG_EVENT(app, "IDLE_LOCK");
    scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
}

static void flippass_idle_tick(App* app) {
    furi_assert(app);

    if(app->last_interaction_tick == 0U || app->scene_manager == NULL) {
        return;
    }

    const uint32_t elapsed_ticks = furi_get_tick() - app->last_interaction_tick;
    const bool progress_active = app->progress_started_tick != 0U;
    if(app->idle_exit_minutes > 0U && !progress_active &&
       elapsed_ticks >= flippass_ms_to_ticks(flippass_minutes_to_ms(app->idle_exit_minutes))) {
        FLIPPASS_LOG_EVENT(app, "IDLE_EXIT");
        flippass_request_exit(app);
        return;
    }

    const uint32_t current_scene = scene_manager_get_current_scene(app->scene_manager);
    if(app->idle_lock_minutes > 0U &&
       elapsed_ticks >= flippass_ms_to_ticks(flippass_minutes_to_ms(app->idle_lock_minutes)) &&
       flippass_idle_lock_can_prompt(app, current_scene)) {
        flippass_idle_request_lock(app);
    }
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

#if FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS
static void flippass_memory_loaded_modules(const App* app, char* out, size_t out_size) {
    size_t used = 0U;
    bool any = false;

    if(out == NULL || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if(app == NULL) {
        snprintf(out, out_size, "-");
        return;
    }

    for(size_t index = 0U; index < FlipPassModuleSlotCount; index++) {
        const FlipPassModuleInstance* instance = &app->module_loader.slot[index];
        if(instance->descriptor == NULL) {
            continue;
        }

        const char* name = flippass_module_slot_name((FlipPassModuleSlot)index);
        const int written = snprintf(
            out + used, out_size - used, "%s%s", any ? "," : "", name != NULL ? name : "unknown");
        if(written < 0) {
            break;
        }

        const size_t written_size = (size_t)written;
        if(written_size >= (out_size - used)) {
            used = out_size - 1U;
            break;
        }

        used += written_size;
        any = true;
    }

    if(!any) {
        snprintf(out, out_size, "none");
    }
}
#endif

void flippass_memory_log(App* app, const char* stage, size_t theoretical_bytes) {
#if !(FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS)
    UNUSED(app);
    UNUSED(stage);
    UNUSED(theoretical_bytes);
#else
    char loaded_modules[160];
    flippass_memory_loaded_modules(app, loaded_modules, sizeof(loaded_modules));
    FLIPPASS_LOG_EVENT(
        app,
        "MEMORY stage=%s free=%lu max=%lu theoretical=%lu loaded=%s",
        stage != NULL ? stage : "unknown",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)theoretical_bytes,
        loaded_modules);
#endif
}

void flippass_memory_log_module(
    App* app,
    const char* stage,
    FlipPassModuleSlot slot,
    size_t theoretical_bytes) {
#if !(FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS)
    UNUSED(app);
    UNUSED(stage);
    UNUSED(slot);
    UNUSED(theoretical_bytes);
#else
    char loaded_modules[160];
    flippass_memory_loaded_modules(app, loaded_modules, sizeof(loaded_modules));
    FLIPPASS_LOG_EVENT(
        app,
        "MEMORY stage=%s slot=%s free=%lu max=%lu theoretical=%lu loaded=%s",
        stage != NULL ? stage : "unknown",
        flippass_module_slot_name(slot),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)theoretical_bytes,
        loaded_modules);
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

static bool
    flippass_keyboard_layout_file_is_valid_with_storage(Storage* storage, const char* path) {
    if(path == NULL || path[0] == '\0' || strcmp(path, FLIPPASS_KEYBOARD_LAYOUT_ALT) == 0) {
        return false;
    }

    FileInfo file_info;
    return storage != NULL && storage_common_stat(storage, path, &file_info) == FSE_OK &&
           !(file_info.flags & FSF_DIRECTORY) && file_info.size == 256U;
}

static bool flippass_load_badusb_keyboard_layout(Storage* storage, FuriString* out_path) {
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* value = furi_string_alloc();
    uint32_t version = 0U;
    bool loaded = false;

    if(flipper_format_file_open_existing(file, FLIPPASS_BADUSB_SETTINGS_FILE_PATH)) {
        if(flipper_format_read_header(file, value, &version) &&
           strcmp(furi_string_get_cstr(value), FLIPPASS_BADUSB_SETTINGS_FILE_TYPE) == 0 &&
           version == FLIPPASS_BADUSB_SETTINGS_VERSION &&
           flipper_format_read_string(file, "layout", value) &&
           flippass_keyboard_layout_file_is_valid_with_storage(
               storage, furi_string_get_cstr(value))) {
            furi_string_set(out_path, value);
            loaded = true;
        }
    }

    if(!loaded && flippass_keyboard_layout_file_is_valid_with_storage(
                      storage, FLIPPASS_BADUSB_SETTINGS_DEFAULT_LAYOUT)) {
        furi_string_set_str(out_path, FLIPPASS_BADUSB_SETTINGS_DEFAULT_LAYOUT);
        loaded = true;
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_string_free(value);
    return loaded;
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
    app->database_cipher = FlipPassKdbxCipherAes256;
    app->database_compression = KDBX_COMPRESSION_GZIP;
    app->database_kdf_rounds = FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    flippass_session_clear_credentials(app);
    app->idle_lock_active = false;
    app->idle_lock_failed_attempts = 0U;
    app->browser_selected_index = 0;
    app->action_selected_index = 0;
    app->other_field_selected_index = 0;
    app->other_field_action_selected_index = 0;
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_otp_kind = FlipPassOtpKindNone;
    app->pending_other_field_name[0] = '\0';
    app->close_db_dialog_open = false;
    app->parse_failed = false;
    app->database_loaded = false;
    app->database_dirty = false;
    app->database_new = false;
    app->pending_vault_fallback = false;
    app->allow_ext_vault_promotion = false;
    app->editor_group = NULL;
    app->editor_entry = NULL;
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

void flippass_record_successful_open(App* app) {
    furi_assert(app);

    if(app->file_path == NULL || furi_string_empty(app->file_path)) {
        return;
    }

    if(app->last_open_file_path != NULL &&
       furi_string_cmp(app->last_open_file_path, app->file_path) == 0) {
        if(app->last_open_count < UINT32_MAX) {
            app->last_open_count++;
        }
    } else {
        furi_string_set(app->last_open_file_path, app->file_path);
        app->last_open_count = 1U;
    }

    flippass_save_settings(app);
}

void flippass_save_settings(App* app) {
    furi_assert(app);
    FLIPPASS_MEMORY_LOG(app, "settings_begin", 0U);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FLIPPASS_MEMORY_LOG(app, "settings_storage_open", 0U);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    FLIPPASS_MEMORY_LOG(app, "settings_after_mkdir", 0U);
    flippass_secure_delete_file_with_storage(storage, FLIPPASS_CONFIG_FILE_PATH);
    FLIPPASS_MEMORY_LOG(app, "settings_after_delete", 0U);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    char last_open_count_text[16];
    char otp_time_zone_text[8];
    char idle_lock_text[8];
    char idle_attempts_text[8];
    char idle_exit_text[8];
    FLIPPASS_MEMORY_LOG(app, "settings_format_alloc", sizeof(last_open_count_text));

    if(flipper_format_file_open_always(file, FLIPPASS_CONFIG_FILE_PATH)) {
        const bool keyboard_layout_configured = app->keyboard_layout_configured;
        FLIPPASS_MEMORY_LOG(app, "settings_file_open", sizeof(last_open_count_text));
        flipper_format_write_bool(
            file, "keyboard_layout_configured", &keyboard_layout_configured, 1U);
        if(keyboard_layout_configured) {
            flipper_format_write_string_cstr(
                file,
                "keyboard_layout",
                (app->keyboard_layout_path != NULL &&
                 !furi_string_empty(app->keyboard_layout_path)) ?
                    furi_string_get_cstr(app->keyboard_layout_path) :
                    FLIPPASS_KEYBOARD_LAYOUT_ALT);
            FLIPPASS_MEMORY_LOG(app, "settings_layout_written", 0U);
        } else {
            FLIPPASS_MEMORY_LOG(app, "settings_layout_badusb_default", 0U);
        }
        if(app->last_open_file_path != NULL && !furi_string_empty(app->last_open_file_path)) {
            snprintf(
                last_open_count_text,
                sizeof(last_open_count_text),
                "%lu",
                (unsigned long)app->last_open_count);
            flippass_secure_write_device_encrypted_string(
                file, "last_file_path", furi_string_get_cstr(app->last_open_file_path));
            FLIPPASS_MEMORY_LOG(app, "settings_path_written", 0U);
            flippass_secure_write_device_encrypted_string(
                file, "last_open_count", last_open_count_text);
            FLIPPASS_MEMORY_LOG(app, "settings_count_written", 0U);
        }
        snprintf(
            otp_time_zone_text,
            sizeof(otp_time_zone_text),
            "%ld",
            (long)app->otp_time_zone_minutes);
        flipper_format_write_string_cstr(file, "otp_time_zone_minutes", otp_time_zone_text);
        {
            const bool always_allow_ext = app->always_allow_ext;
            flipper_format_write_bool(file, "always_allow_ext", &always_allow_ext, 1U);
        }
        snprintf(
            idle_lock_text, sizeof(idle_lock_text), "%u", (unsigned int)app->idle_lock_minutes);
        flipper_format_write_string_cstr(file, "idle_lock_minutes", idle_lock_text);
        snprintf(
            idle_attempts_text,
            sizeof(idle_attempts_text),
            "%u",
            (unsigned int)app->idle_unlock_attempts);
        flipper_format_write_string_cstr(file, "idle_unlock_attempts", idle_attempts_text);
        snprintf(
            idle_exit_text, sizeof(idle_exit_text), "%u", (unsigned int)app->idle_exit_minutes);
        flipper_format_write_string_cstr(file, "idle_exit_minutes", idle_exit_text);
    }

    flipper_format_file_close(file);
    FLIPPASS_MEMORY_LOG(app, "settings_file_closed", 0U);
    flipper_format_free(file);
    FLIPPASS_MEMORY_LOG(app, "settings_format_free", 0U);
    furi_record_close(RECORD_STORAGE);
    FLIPPASS_MEMORY_LOG(app, "settings_done", 0U);
}

static void flippass_load_settings(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* keyboard_layout = furi_string_alloc();
    FuriString* legacy_last_file_path = furi_string_alloc();
    FuriString* last_open_count = furi_string_alloc();
    FuriString* otp_time_zone = furi_string_alloc();
    FuriString* idle_lock = furi_string_alloc();
    FuriString* idle_attempts = furi_string_alloc();
    FuriString* idle_exit = furi_string_alloc();
    bool stored_keyboard_layout_configured = false;
    bool has_keyboard_layout_configured = false;
    bool has_valid_keyboard_layout = false;
    bool keyboard_layout_is_alt = false;

    furi_string_reset(app->last_open_file_path);
    app->last_open_count = 0U;
    app->keyboard_layout_configured = false;
    furi_string_reset(app->keyboard_layout_path);
    app->otp_time_zone_minutes = 0;
    app->always_allow_ext = false;
    app->idle_lock_minutes = FLIPPASS_DEFAULT_IDLE_LOCK_MINUTES;
    app->idle_unlock_attempts = FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS;
    app->idle_exit_minutes = FLIPPASS_DEFAULT_IDLE_EXIT_MINUTES;

    if(flipper_format_file_open_existing(file, FLIPPASS_CONFIG_FILE_PATH)) {
        flipper_format_rewind(file);
        if(!flippass_secure_read_device_encrypted_string(
               file, "last_file_path", app->last_open_file_path, true, NULL)) {
            flipper_format_rewind(file);
            if(flipper_format_read_string(file, "last_file_path", legacy_last_file_path) &&
               !furi_string_empty(legacy_last_file_path)) {
                furi_string_set(app->last_open_file_path, legacy_last_file_path);
            }
        }

        flipper_format_rewind(file);
        if(flippass_secure_read_device_encrypted_string(
               file, "last_open_count", last_open_count, true, NULL) &&
           !furi_string_empty(last_open_count)) {
            char* parse_end = NULL;
            const unsigned long parsed =
                strtoul(furi_string_get_cstr(last_open_count), &parse_end, 10);
            if(parse_end != NULL && *parse_end == '\0' && parsed <= UINT32_MAX) {
                app->last_open_count = (uint32_t)parsed;
            }
        }

        flipper_format_rewind(file);
        has_keyboard_layout_configured = flipper_format_read_bool(
            file, "keyboard_layout_configured", &stored_keyboard_layout_configured, 1U);

        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "keyboard_layout", keyboard_layout) &&
           !furi_string_empty(keyboard_layout)) {
            const char* layout_path = furi_string_get_cstr(keyboard_layout);
            keyboard_layout_is_alt = strcmp(layout_path, FLIPPASS_KEYBOARD_LAYOUT_ALT) == 0;
            if(keyboard_layout_is_alt) {
                has_valid_keyboard_layout = true;
                furi_string_reset(app->keyboard_layout_path);
            } else if(flippass_keyboard_layout_file_is_valid_with_storage(storage, layout_path)) {
                has_valid_keyboard_layout = true;
                furi_string_set(app->keyboard_layout_path, keyboard_layout);
            } else {
                furi_string_reset(app->keyboard_layout_path);
            }
        }

        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "otp_time_zone_minutes", otp_time_zone) &&
           !furi_string_empty(otp_time_zone)) {
            char* parse_end = NULL;
            const long parsed = strtol(furi_string_get_cstr(otp_time_zone), &parse_end, 10);
            if(parse_end != NULL && *parse_end == '\0' &&
               parsed >= FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES &&
               parsed <= FLIPPASS_OTP_TIME_ZONE_MAX_MINUTES &&
               (parsed % FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES) == 0) {
                app->otp_time_zone_minutes = (int16_t)parsed;
            }
        } else {
            flipper_format_rewind(file);
            if(flipper_format_read_string(file, "otp_time_zone_hours", otp_time_zone) &&
               !furi_string_empty(otp_time_zone)) {
                char* parse_end = NULL;
                const long parsed = strtol(furi_string_get_cstr(otp_time_zone), &parse_end, 10);
                if(parse_end != NULL && *parse_end == '\0' && parsed >= -12 && parsed <= 14) {
                    app->otp_time_zone_minutes = (int16_t)(parsed * 60);
                }
            }
        }

        flipper_format_rewind(file);
        flipper_format_read_bool(file, "always_allow_ext", &app->always_allow_ext, 1U);

        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "idle_lock_minutes", idle_lock) &&
           !furi_string_empty(idle_lock)) {
            char* parse_end = NULL;
            const unsigned long parsed = strtoul(furi_string_get_cstr(idle_lock), &parse_end, 10);
            if(parse_end != NULL && *parse_end == '\0' && parsed <= UINT16_MAX) {
                app->idle_lock_minutes = (uint16_t)parsed;
            }
        }

        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "idle_unlock_attempts", idle_attempts) &&
           !furi_string_empty(idle_attempts)) {
            char* parse_end = NULL;
            const unsigned long parsed =
                strtoul(furi_string_get_cstr(idle_attempts), &parse_end, 10);
            if(parse_end != NULL && *parse_end == '\0' && parsed >= 1U && parsed <= UINT8_MAX) {
                app->idle_unlock_attempts = (uint8_t)parsed;
            }
        }

        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "idle_exit_minutes", idle_exit) &&
           !furi_string_empty(idle_exit)) {
            char* parse_end = NULL;
            const unsigned long parsed = strtoul(furi_string_get_cstr(idle_exit), &parse_end, 10);
            if(parse_end != NULL && *parse_end == '\0' && parsed <= UINT16_MAX) {
                app->idle_exit_minutes = (uint16_t)parsed;
            }
        }
    }

    if(has_keyboard_layout_configured) {
        app->keyboard_layout_configured = stored_keyboard_layout_configured &&
                                          has_valid_keyboard_layout;
    } else {
        app->keyboard_layout_configured = has_valid_keyboard_layout && !keyboard_layout_is_alt;
    }

    if(!app->keyboard_layout_configured &&
       !flippass_load_badusb_keyboard_layout(storage, app->keyboard_layout_path)) {
        furi_string_reset(app->keyboard_layout_path);
    }

    if(furi_string_empty(app->last_open_file_path)) {
        app->last_open_count = 0U;
    }
    furi_string_set(app->file_path, app->last_open_file_path);

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(keyboard_layout);
    furi_string_free(legacy_last_file_path);
    furi_string_free(last_open_count);
    furi_string_free(otp_time_zone);
    furi_string_free(idle_lock);
    furi_string_free(idle_attempts);
    furi_string_free(idle_exit);
}

#if FLIPPASS_ENABLE_DEBUG_CREATE_HOOK
static bool flippass_try_debug_create(App* app) {
    static const char* root_name = "debug_gzip_create";
    static const char* password = "Test123";
    FuriString* error = furi_string_alloc();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;

    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    storage_simply_remove(storage, FLIPPASS_DEBUG_CREATE_FILE_PATH);

    ok = flippass_db_create_new_database(
             app, root_name, FlipPassKdbxCipherAes256, KDBX_COMPRESSION_GZIP, error) &&
         flippass_save_execute(
             app,
             FLIPPASS_DEBUG_CREATE_FILE_PATH,
             password,
             FlipPassKdbxCipherAes256,
             KDBX_COMPRESSION_GZIP,
             FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS,
             error);

    if(ok) {
        FLIPPASS_LOG_EVENT(app, "DEBUG_CREATE_HOOK_OK path=%s", FLIPPASS_DEBUG_CREATE_FILE_PATH);
        flippass_scene_status_show(
            app, "Debug Create", "Created debug GZip KDBX.", FlipPassScene_FileBrowser);
    } else {
        const char* detail = (error != NULL && !furi_string_empty(error)) ?
                                 furi_string_get_cstr(error) :
                                 "Debug create failed.";
        FLIPPASS_LOG_EVENT(app, "DEBUG_CREATE_HOOK_FAIL reason=%s", detail);
        flippass_scene_status_show(app, "Debug Create Failed", detail, FlipPassScene_FileBrowser);
    }

    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(error);
    return true;
}
#endif

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
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, flippass_tick_event_callback, FLIPPASS_SCENE_TICK_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->file_path = furi_string_alloc();
    app->browser_directory = furi_string_alloc();
    app->pending_path = furi_string_alloc();
    app->keyboard_layout_path = furi_string_alloc();
    app->last_open_file_path = furi_string_alloc();
    app->last_open_count = 0U;
    app->keyboard_layout_configured = false;
    app->otp_time_zone_minutes = 0;
    app->always_allow_ext = false;

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

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

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
    app->database_cipher = FlipPassKdbxCipherAes256;
    app->database_compression = KDBX_COMPRESSION_GZIP;
    app->database_kdf_rounds = FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    flippass_session_clear_credentials(app);
    app->text_view_body = furi_string_alloc();
    app->usb_expect_rpc_session_close = false;
    app->browser_selected_index = 0;
    app->action_selected_index = 0;
    app->other_field_selected_index = 0;
    app->other_field_action_selected_index = 0;
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_otp_kind = FlipPassOtpKindNone;
    app->pending_other_field_name[0] = '\0';
    app->keyboard_layout_return_scene = FlipPassScene_DbEntries;
    app->close_db_dialog_open = false;
    app->parse_failed = false;
    app->database_loaded = false;
    app->database_dirty = false;
    app->database_new = false;
    app->pending_vault_fallback = false;
    app->allow_ext_vault_promotion = false;
    app->close_test_logged = false;
    app->status_return_scene = FlipPassScene_FileBrowser;
    app->text_view_title[0] = '\0';
    app->text_view_return_scene = FlipPassScene_DbEntries;
    app->progress_started_tick = 0U;
    app->progress_percent = 0U;
    app->progress_title[0] = '\0';
    app->editor_mode = FlipPassEditorModeNone;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_custom_fields = NULL;
    app->editor_custom_field_draft = NULL;
    app->editor_custom_field = NULL;
    app->editor_custom_field_protected = false;
    app->editor_custom_field_name[0] = '\0';
    app->editor_custom_field_value[0] = '\0';
    app->editor_otp_kind = FlipPassOtpKindTime;
    app->editor_otp_secret_encoding = FlipPassOtpSecretEncodingBase32;
    app->editor_otp_algorithm = FlipPassOtpAlgorithmSha1;
    app->editor_otp_digits = FLIPPASS_OTP_DEFAULT_DIGITS;
    app->editor_otp_period = FLIPPASS_OTP_DEFAULT_PERIOD;
    app->editor_otp_time_zone_minutes = 0;
    app->editor_otp_settled = false;
    app->editor_otp_secret[0] = '\0';
    snprintf(
        app->editor_otp_counter,
        sizeof(app->editor_otp_counter),
        "%llu",
        (unsigned long long)FLIPPASS_OTP_DEFAULT_COUNTER);
    app->password_gen_target = FlipPassPasswordGenTargetNone;
    app->password_gen_charset = FlipPassPasswordGenCharsetFull;
    app->password_gen_length = 20U;
    app->password_gen_harvest_seconds = 10U;
    app->password_gen_selected_index = 0U;
    app->password_gen_started_tick = 0U;
    app->password_gen_capture_active = false;
    app->password_gen_auto_open_field_name = false;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_idle_lock_minutes = FLIPPASS_DEFAULT_IDLE_LOCK_MINUTES;
    app->editor_idle_unlock_attempts = FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS;
    app->editor_idle_exit_minutes = FLIPPASS_DEFAULT_IDLE_EXIT_MINUTES;
    app->editor_always_allow_ext = false;
    app->editor_keyboard_layout_index = 0U;
    app->editor_keyboard_layout_use_alt = true;
    app->editor_keyboard_layout_available = false;
    app->editor_keyboard_layout_path[0] = '\0';
    app->browser_directory_selected_index = 0U;
    app->browser_menu_selected_index = 0U;
    app->editor_close_after_commit = false;
    app->rpc = NULL;
    app->rpc_mode = false;
    app->typing_active = false;
    app->typing_cancel_requested = false;
    app->typing_cancel_back_tick = 0U;
    app->idle_lock_active = false;
    app->idle_lock_failed_attempts = 0U;
    app->idle_lock_minutes = FLIPPASS_DEFAULT_IDLE_LOCK_MINUTES;
    app->idle_unlock_attempts = FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS;
    app->idle_exit_minutes = FLIPPASS_DEFAULT_IDLE_EXIT_MINUTES;
    flippass_update_last_interaction(app);
    flippass_module_loader_init(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    flippass_set_status(app, "FlipPass", "");
    flippass_log_reset(app);
    flippass_system_log_capture_init(app);
    FLIPPASS_LOG_EVENT(app, "APP_START");
    FLIPPASS_MEMORY_LOG(app, "app_start", sizeof(App));
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

#if FLIPPASS_ENABLE_DEBUG_CREATE_HOOK
    const bool debug_create_ran = flippass_try_debug_create(app);
#else
    const bool debug_create_ran = false;
#endif

    // If a valid last file is found, transition to the password entry scene
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool has_last_file = !debug_create_ran && app->file_path != NULL &&
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

    view_dispatcher_remove_view(app->view_dispatcher, AppViewPasswordEntry);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewDbBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewDialogEx);
    dialog_ex_free(app->dialog_ex);
    widget_free(app->widget);
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    flippass_db_browser_view_free(app->db_browser);
    flippass_progress_view_free(app->progress_view);
    text_input_free(app->text_input);
    furi_string_free(app->file_path);
    furi_string_free(app->browser_directory);
    furi_string_free(app->pending_path);
    furi_string_free(app->keyboard_layout_path);
    furi_string_free(app->last_open_file_path);
    flippass_editor_clear_custom_field_drafts(app);
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
