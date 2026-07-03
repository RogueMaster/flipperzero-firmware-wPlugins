#include "flippass_save_plugin.h"

#include "../kdbx/aes.h"
#include "../kdbx/hmac.h"
#include "../kdbx/kdbx_constants.h"
#include "../kdbx/memzero.h"
#include "../kdbx/sha2.h"

#include <storage/storage.h>
#include <toolbox/path.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_SAVE_HEADER_CAPACITY 256U

static const uint8_t KDBX_UUID_AES_KDF[] =
    {0xC9, 0xD9, 0xF3, 0x9A, 0x62, 0x8A, 0x44, 0x60, 0xBF, 0x74, 0x0D, 0x08, 0xC1, 0x8A, 0x4F, 0xEA};

typedef struct {
    Storage* storage;
    File* file;
    FuriString* temp_path;
    FuriString* dir_path;
    FuriString* error_detail;
    const FlipPassSaveHeaderRequestV1* request;
    const FlipPassSaveStageHostApiV1* host_api;
    bool use_aes;
} FlipPassSaveHeaderContext;

typedef struct {
    uint8_t master_seed[32];
    uint8_t kdf_salt[32];
    uint8_t header[FLIPPASS_SAVE_HEADER_CAPACITY];
    uint8_t iv[16];
    uint8_t cipher_key[32];
    uint8_t header_sha[32];
    uint8_t header_hmac_input[72];
    uint8_t header_hmac_key[64];
    uint8_t header_hmac[32];
} FlipPassSaveHeaderWork;

static void flippass_save_header_progress(
    FlipPassSaveHeaderContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->progress != NULL) {
        ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
    }
}

#if FLIPPASS_ENABLE_LOGS
static void flippass_save_header_log_heap_raw(
    const FlipPassSaveStageHostApiV1* host_api,
    const char* stage) {
    char message[112];

    if(host_api == NULL || host_api->log == NULL) {
        return;
    }

    snprintf(
        message,
        sizeof(message),
        "SAVE_HEADER stage=%s free=%lu max=%lu",
        stage != NULL ? stage : "unknown",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    host_api->log(host_api->context, message);
}

static void flippass_save_header_log_heap(FlipPassSaveHeaderContext* ctx, const char* stage) {
    if(ctx == NULL) {
        return;
    }

    flippass_save_header_log_heap_raw(ctx->host_api, stage);
}
#else
static void flippass_save_header_log_heap_raw(
    const FlipPassSaveStageHostApiV1* host_api,
    const char* stage) {
    UNUSED(host_api);
    UNUSED(stage);
}

static void flippass_save_header_log_heap(FlipPassSaveHeaderContext* ctx, const char* stage) {
    UNUSED(ctx);
    UNUSED(stage);
}
#endif

static uint8_t flippass_save_header_progress_range(
    uint8_t start,
    uint8_t end,
    uint64_t completed,
    uint64_t total) {
    const uint8_t span = (end > start) ? (uint8_t)(end - start) : 0U;

    if(total == 0U || completed >= total) {
        return end;
    }

    return (uint8_t)(start + (uint8_t)((completed * span) / total));
}

static void flippass_save_header_write_u32_le(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
    out[2] = (uint8_t)((value >> 16) & 0xFFU);
    out[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void flippass_save_header_write_u64_le(uint8_t* out, uint64_t value) {
    for(size_t index = 0U; index < 8U; index++) {
        out[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static void flippass_save_header_set_error(
    FlipPassSaveHeaderContext* ctx,
    FuriString* error,
    const char* message) {
    furi_assert(ctx);
    furi_assert(error);
    furi_assert(message);

    furi_string_set_str(error, message);
    furi_string_set_str(ctx->error_detail, message);
}

static bool flippass_save_header_file_write(
    FlipPassSaveHeaderContext* ctx,
    const void* data,
    size_t data_size,
    FuriString* error) {
    if(data_size == 0U) {
        return true;
    }

    if(storage_file_write(ctx->file, data, data_size) != data_size) {
        flippass_save_header_set_error(
            ctx, error, "FlipPass could not write the target KDBX header.");
        return false;
    }

    return true;
}

static bool flippass_save_header_open_target(FlipPassSaveHeaderContext* ctx, FuriString* error) {
    path_extract_dirname(ctx->request->file_path, ctx->dir_path);
    storage_simply_mkdir(ctx->storage, furi_string_get_cstr(ctx->dir_path));
    furi_string_printf(ctx->temp_path, "%s.tmp", ctx->request->file_path);
    storage_simply_remove(ctx->storage, furi_string_get_cstr(ctx->temp_path));

    ctx->file = storage_file_alloc(ctx->storage);
    if(ctx->file == NULL ||
       !storage_file_open(
           ctx->file, furi_string_get_cstr(ctx->temp_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        flippass_save_header_set_error(
            ctx, error, "FlipPass could not create the temporary KDBX file.");
        return false;
    }

    return true;
}

static bool flippass_save_header_build_kdf_parameters(
    const FlipPassSaveHeaderRequestV1* request,
    const uint8_t salt[32],
    uint8_t* out,
    size_t capacity,
    size_t* out_size) {
    size_t offset = 0U;

    furi_assert(request);
    furi_assert(salt);
    furi_assert(out);
    furi_assert(out_size);

    if(capacity < 80U) {
        return false;
    }

    out[offset++] = 0x00U;
    out[offset++] = 0x01U;

    out[offset++] = 0x42U;
    flippass_save_header_write_u32_le(out + offset, 5U);
    offset += 4U;
    memcpy(out + offset, "$UUID", 5U);
    offset += 5U;
    flippass_save_header_write_u32_le(out + offset, sizeof(KDBX_UUID_AES_KDF));
    offset += 4U;
    memcpy(out + offset, KDBX_UUID_AES_KDF, sizeof(KDBX_UUID_AES_KDF));
    offset += sizeof(KDBX_UUID_AES_KDF);

    out[offset++] = 0x42U;
    flippass_save_header_write_u32_le(out + offset, 1U);
    offset += 4U;
    out[offset++] = 'S';
    flippass_save_header_write_u32_le(out + offset, 32U);
    offset += 4U;
    memcpy(out + offset, salt, 32U);
    offset += 32U;

    out[offset++] = 0x05U;
    flippass_save_header_write_u32_le(out + offset, 1U);
    offset += 4U;
    out[offset++] = 'R';
    flippass_save_header_write_u32_le(out + offset, 8U);
    offset += 4U;
    flippass_save_header_write_u64_le(out + offset, request->kdf_rounds);
    offset += 8U;

    out[offset++] = 0x00U;
    *out_size = offset;
    return true;
}

static bool flippass_save_header_build_header(
    const FlipPassSaveHeaderRequestV1* request,
    const uint8_t master_seed[32],
    const uint8_t kdf_salt[32],
    const uint8_t* iv,
    size_t iv_size,
    uint8_t* out,
    size_t capacity,
    size_t* out_size) {
    uint8_t kdf[96];
    size_t kdf_size = 0U;
    size_t offset = 0U;
    const uint32_t version = 0x00040001U;

    furi_assert(request);
    furi_assert(master_seed);
    furi_assert(kdf_salt);
    furi_assert(iv);
    furi_assert(out);
    furi_assert(out_size);

    if(!flippass_save_header_build_kdf_parameters(request, kdf_salt, kdf, sizeof(kdf), &kdf_size) ||
       capacity < (128U + kdf_size)) {
        return false;
    }

    flippass_save_header_write_u32_le(out + offset, KDBX_SIGNATURE_1);
    offset += 4U;
    flippass_save_header_write_u32_le(out + offset, KDBX_SIGNATURE_2);
    offset += 4U;
    flippass_save_header_write_u32_le(out + offset, version);
    offset += 4U;

    out[offset++] = KDBX_HEADER_FIELD_ID_ENCRYPTION_ALGORITHM;
    flippass_save_header_write_u32_le(out + offset, 16U);
    offset += 4U;
    memcpy(
        out + offset,
        (request->cipher == FlipPassSaveCipherChaCha20) ? KDBX_UUID_CHACHA20 : KDBX_UUID_AES256,
        16U);
    offset += 16U;

    out[offset++] = KDBX_HEADER_FIELD_ID_COMPRESSION_ALGORITHM;
    flippass_save_header_write_u32_le(out + offset, 4U);
    offset += 4U;
    flippass_save_header_write_u32_le(out + offset, request->compression);
    offset += 4U;

    out[offset++] = KDBX_HEADER_FIELD_ID_MASTER_SEED;
    flippass_save_header_write_u32_le(out + offset, 32U);
    offset += 4U;
    memcpy(out + offset, master_seed, 32U);
    offset += 32U;

    out[offset++] = KDBX_HEADER_FIELD_ID_ENCRYPTION_IV;
    flippass_save_header_write_u32_le(out + offset, (uint32_t)iv_size);
    offset += 4U;
    memcpy(out + offset, iv, iv_size);
    offset += iv_size;

    out[offset++] = KDBX_HEADER_FIELD_ID_KDF_PARAMETERS;
    flippass_save_header_write_u32_le(out + offset, (uint32_t)kdf_size);
    offset += 4U;
    memcpy(out + offset, kdf, kdf_size);
    offset += kdf_size;

    out[offset++] = KDBX_HEADER_FIELD_ID_END;
    flippass_save_header_write_u32_le(out + offset, 4U);
    offset += 4U;
    out[offset++] = 0x0DU;
    out[offset++] = 0x0AU;
    out[offset++] = 0x0DU;
    out[offset++] = 0x0AU;

    *out_size = offset;
    memzero(kdf, sizeof(kdf));
    return true;
}

static bool flippass_save_header_can_reuse_transformed(
    const FlipPassSaveHeaderRequestV1* request,
    const uint8_t kdf_salt[32]) {
    return request != NULL && request->transformed_key != NULL &&
           request->transformed_key_size == 32U && request->kdf_salt != NULL &&
           request->kdf_salt_size == 32U && memcmp(request->kdf_salt, kdf_salt, 32U) == 0;
}

static bool flippass_save_header_derive_keys(
    FlipPassSaveHeaderContext* ctx,
    const uint8_t master_seed[32],
    const uint8_t kdf_salt[32],
    uint8_t cipher_key[32],
    uint8_t hmac_base[64],
    uint8_t transformed_key_out[32],
    FuriString* error) {
    const FlipPassSaveHeaderRequestV1* request = NULL;
    uint8_t composite_key[32];
    uint8_t transformed_key[32];
    uint8_t material[65];
    aes_encrypt_ctx* aes_ctx = NULL;
    uint64_t progress_interval = 0U;
    char detail[40];

    furi_assert(ctx);
    request = ctx->request;
    furi_assert(request);
    furi_assert(master_seed);
    furi_assert(kdf_salt);
    furi_assert(cipher_key);
    furi_assert(hmac_base);
    furi_assert(transformed_key_out);
    furi_assert(error);

    memzero(composite_key, sizeof(composite_key));
    memzero(transformed_key, sizeof(transformed_key));
    memzero(transformed_key_out, 32U);

    if(flippass_save_header_can_reuse_transformed(request, kdf_salt)) {
        memcpy(transformed_key, request->transformed_key, sizeof(transformed_key));
        flippass_save_header_log_heap(ctx, "derive_reuse_transformed");
        flippass_save_header_progress(ctx, "Key Derivation", "Reusing session key", 34U);
        goto material_ready;
    }

    if(request->composite_key == NULL || request->composite_key_size != sizeof(composite_key)) {
        furi_string_set_str(error, "FlipPass save key is unavailable.");
        return false;
    }
    memcpy(composite_key, request->composite_key, sizeof(composite_key));

    flippass_save_header_log_heap(ctx, "derive_enter");
    aes_ctx = malloc(sizeof(*aes_ctx));
    if(aes_ctx == NULL) {
        furi_string_set_str(error, "Not enough RAM is available for AES-KDF save.");
        memzero(composite_key, sizeof(composite_key));
        return false;
    }

    progress_interval = request->kdf_rounds / 40U;
    if(progress_interval < 10000U) {
        progress_interval = 10000U;
    }
    snprintf(detail, sizeof(detail), "Rounds 0%%");
    flippass_save_header_progress(ctx, "Key Derivation", detail, 8U);

    if(aes_encrypt_key256(kdf_salt, aes_ctx) != EXIT_SUCCESS) {
        furi_string_set_str(error, "FlipPass could not initialize the KDBX AES-KDF.");
        memzero(composite_key, sizeof(composite_key));
        memzero(aes_ctx, sizeof(*aes_ctx));
        free(aes_ctx);
        return false;
    }

    for(uint64_t round = 0U; round < request->kdf_rounds; round++) {
        aes_encrypt(composite_key, composite_key, aes_ctx);
        aes_encrypt(composite_key + 16U, composite_key + 16U, aes_ctx);
        const uint64_t completed = round + 1U;
        if(completed == request->kdf_rounds || (completed % progress_interval) == 0U) {
            snprintf(
                detail,
                sizeof(detail),
                "Rounds %u%%",
                (unsigned int)flippass_save_header_progress_range(
                    0U, 100U, completed, request->kdf_rounds));
            flippass_save_header_progress(
                ctx,
                "Key Derivation",
                detail,
                flippass_save_header_progress_range(8U, 34U, completed, request->kdf_rounds));
            furi_thread_yield();
        }
    }
    flippass_save_header_log_heap(ctx, "derive_rounds_ok");

    sha256_Raw(composite_key, sizeof(composite_key), transformed_key);

material_ready:
    memcpy(material, master_seed, 32U);
    memcpy(material + 32U, transformed_key, 32U);
    sha256_Raw(material, 64U, cipher_key);
    material[64U] = 1U;
    sha512_Raw(material, sizeof(material), hmac_base);
    memcpy(transformed_key_out, transformed_key, 32U);

    memzero(composite_key, sizeof(composite_key));
    memzero(transformed_key, sizeof(transformed_key));
    memzero(material, sizeof(material));
    if(aes_ctx != NULL) {
        memzero(aes_ctx, sizeof(*aes_ctx));
        free(aes_ctx);
    }
    return true;
}

static bool flippass_save_header_run_internal(
    FlipPassSaveHeaderContext* ctx,
    FlipPassSaveHeaderResultV1* result,
    FuriString* error) {
    FlipPassSaveHeaderWork* work = NULL;
    size_t header_size = 0U;
    const size_t iv_size = ctx->use_aes ? 16U : 12U;
    bool ok = false;

    flippass_save_header_log_heap(ctx, "run_internal_enter");
    flippass_save_header_progress(ctx, "Preparing Target", "Opening temp file", 2U);

    if(!flippass_save_header_open_target(ctx, error)) {
        goto cleanup;
    }
    flippass_save_header_log_heap(ctx, "target_open_ok");

    flippass_save_header_progress(ctx, "Preparing Header", "Generating seeds", 6U);
    flippass_save_header_log_heap(ctx, "header_work_enter");
    work = malloc(sizeof(*work));
    if(work == NULL) {
        flippass_save_header_set_error(
            ctx, error, "Not enough RAM is available for KDBX header save.");
        goto cleanup;
    }
    memset(work, 0, sizeof(*work));

    furi_hal_random_fill_buf(work->master_seed, sizeof(work->master_seed));
    if(ctx->request->transformed_key != NULL && ctx->request->transformed_key_size == 32U &&
       ctx->request->kdf_salt != NULL && ctx->request->kdf_salt_size == sizeof(work->kdf_salt)) {
        memcpy(work->kdf_salt, ctx->request->kdf_salt, sizeof(work->kdf_salt));
    } else {
        furi_hal_random_fill_buf(work->kdf_salt, sizeof(work->kdf_salt));
    }
    furi_hal_random_fill_buf(work->iv, iv_size);

    if(!flippass_save_header_build_header(
           ctx->request,
           work->master_seed,
           work->kdf_salt,
           work->iv,
           iv_size,
           work->header,
           sizeof(work->header),
           &header_size) ||
       !flippass_save_header_derive_keys(
           ctx,
           work->master_seed,
           work->kdf_salt,
           work->cipher_key,
           result->hmac_base,
           result->transformed_key,
           error)) {
        flippass_save_header_set_error(ctx, error, "FlipPass could not assemble the KDBX header.");
        goto cleanup;
    }

    flippass_save_header_progress(ctx, "Writing Header", "Header hash and HMAC", 36U);
    memcpy(result->cipher_key, work->cipher_key, sizeof(result->cipher_key));
    memcpy(result->kdf_salt, work->kdf_salt, sizeof(result->kdf_salt));
    memcpy(result->iv, work->iv, iv_size);
    result->iv_size = iv_size;
    result->transformed_key_ready = true;

    sha256_Raw(work->header, header_size, work->header_sha);
    memset(work->header_hmac_input, 0xFF, 8U);
    memcpy(work->header_hmac_input + 8U, result->hmac_base, sizeof(result->hmac_base));
    sha512_Raw(work->header_hmac_input, sizeof(work->header_hmac_input), work->header_hmac_key);
    hmac_sha256(
        work->header_hmac_key,
        sizeof(work->header_hmac_key),
        work->header,
        (uint32_t)header_size,
        work->header_hmac);

    ok = flippass_save_header_file_write(ctx, work->header, header_size, error) &&
         flippass_save_header_file_write(ctx, work->header_sha, sizeof(work->header_sha), error) &&
         flippass_save_header_file_write(ctx, work->header_hmac, sizeof(work->header_hmac), error);
    if(ok) {
        storage_file_sync(ctx->file);
        storage_file_close(ctx->file);
        storage_file_free(ctx->file);
        ctx->file = NULL;
    }
    flippass_save_header_log_heap(ctx, ok ? "header_write_ok" : "header_write_fail");

cleanup:
    if(!ok) {
        memzero(result, sizeof(*result));
        if(ctx->file != NULL) {
            storage_file_close(ctx->file);
            storage_file_free(ctx->file);
            ctx->file = NULL;
        }
        if(ctx->temp_path != NULL && !furi_string_empty(ctx->temp_path)) {
            storage_simply_remove(ctx->storage, furi_string_get_cstr(ctx->temp_path));
        }
    }
    if(work != NULL) {
        memzero(work, sizeof(*work));
        free(work);
    }
    return ok;
}

static bool flippass_save_header_plugin_run(
    const FlipPassSaveHeaderRequestV1* request,
    const FlipPassSaveStageHostApiV1* host_api,
    FlipPassSaveHeaderResultV1* result,
    FuriString* error) {
    FlipPassSaveHeaderContext* ctx = NULL;
    bool ok = false;

    if(request == NULL || host_api == NULL || result == NULL || error == NULL ||
       request->api_version != FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_SAVE_STAGE_HOST_API_VERSION ||
       request->file_path == NULL || request->composite_key == NULL ||
       request->composite_key_size != 32U || request->kdf_rounds == 0U ||
       (request->compression != KDBX_COMPRESSION_NONE &&
        request->compression != KDBX_COMPRESSION_GZIP)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass received an invalid save-header request.");
        }
        return false;
    }

    memzero(result, sizeof(*result));
    flippass_save_header_log_heap_raw(host_api, "run_enter");
    ctx = malloc(sizeof(FlipPassSaveHeaderContext));
    if(ctx == NULL) {
        furi_string_set_str(error, "Not enough RAM is available for the save header.");
        return false;
    }
    memset(ctx, 0, sizeof(FlipPassSaveHeaderContext));

    ctx->storage = furi_record_open(RECORD_STORAGE);
    ctx->temp_path = furi_string_alloc();
    ctx->dir_path = furi_string_alloc();
    ctx->error_detail = furi_string_alloc();
    ctx->request = request;
    ctx->host_api = host_api;
    ctx->use_aes = request->cipher != FlipPassSaveCipherChaCha20;
    flippass_save_header_log_heap(ctx, "context_ready");

    if(ctx->temp_path == NULL || ctx->dir_path == NULL || ctx->error_detail == NULL) {
        furi_string_set_str(error, "Not enough RAM is available for save header paths.");
        goto cleanup;
    }

    ok = flippass_save_header_run_internal(ctx, result, error);

cleanup:
    if(ctx->file != NULL) {
        storage_file_close(ctx->file);
        storage_file_free(ctx->file);
    }
    if(ctx->temp_path != NULL) {
        furi_string_free(ctx->temp_path);
    }
    if(ctx->dir_path != NULL) {
        furi_string_free(ctx->dir_path);
    }
    if(ctx->error_detail != NULL) {
        furi_string_free(ctx->error_detail);
    }
    if(ctx->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    memzero(ctx, sizeof(FlipPassSaveHeaderContext));
    free(ctx);

    return ok;
}

static const FlipPassSaveHeaderPluginV1 flippass_save_header_plugin = {
    .api_version = FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION,
    .run = flippass_save_header_plugin_run,
};

static const FlipperAppPluginDescriptor flippass_save_header_plugin_descriptor = {
    .appid = FLIPPASS_SAVE_HEADER_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION,
    .entry_point = &flippass_save_header_plugin,
};

const FlipperAppPluginDescriptor* flippass_save_header_plugin_ep(void) {
    return &flippass_save_header_plugin_descriptor;
}
