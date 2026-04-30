#include "flippass_open_acquire_plugin.h"

#include "../kdbx/kdbx_parser.h"
#include "../kdbx/memzero.h"
#include "../kdbx/sha2.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const FlipPassOpenAcquireHostApiV1* host_api;
    FuriString* error;
    KDBXParser* parser;
    uint8_t progress_percent;
} FlipPassOpenAcquireContext;

static void fp_open_acquire_progress(
    FlipPassOpenAcquireContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx == NULL || ctx->host_api == NULL || ctx->host_api->progress == NULL) {
        return;
    }

    ctx->progress_percent = percent;
    ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
}

static void fp_open_acquire_log(FlipPassOpenAcquireContext* ctx, const char* message) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->log != NULL && message != NULL) {
        ctx->host_api->log(ctx->host_api->context, message);
    }
}

static void
    fp_open_acquire_kdf_progress(uint64_t current_round, uint64_t total_rounds, void* context) {
    FlipPassOpenAcquireContext* ctx = context;
    char detail[64];
    uint8_t percent = 10U;
    uint32_t round_percent = 0U;

    if(ctx == NULL || total_rounds == 0U) {
        return;
    }

    if(current_round > total_rounds) {
        current_round = total_rounds;
    }

    percent = (uint8_t)(10U + ((current_round * 25U) / total_rounds));
    if(percent > 35U) {
        percent = 35U;
    }
    if(percent <= ctx->progress_percent && current_round != total_rounds) {
        return;
    }

    round_percent = (uint32_t)((current_round * 100U) / total_rounds);
    if(round_percent > 100U) {
        round_percent = 100U;
    }

    snprintf(detail, sizeof(detail), "Rounds %lu%%", (unsigned long)round_percent);
    fp_open_acquire_progress(ctx, "Key Derivation", detail, percent);
}

static bool fp_open_acquire_run(
    const FlipPassOpenAcquireRequestV1* request,
    const FlipPassOpenAcquireHostApiV1* host_api,
    KDBXOpenProfile* out_profile,
    FuriString* error) {
    FlipPassOpenAcquireContext ctx = {0};
    const KDBXHeader* header = NULL;

    if(request == NULL || host_api == NULL || out_profile == NULL || error == NULL ||
       request->api_version != FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_OPEN_ACQUIRE_HOST_API_VERSION ||
       request->file_path == NULL || request->file_path[0] == '\0' || request->password == NULL ||
       request->password[0] == '\0') {
        furi_string_set_str(error, "Open acquire ABI is unavailable or incompatible.");
        return false;
    }

    memzero(out_profile, sizeof(*out_profile));
    ctx.host_api = host_api;
    ctx.error = error;
    ctx.parser = kdbx_parser_alloc();
    if(ctx.parser == NULL) {
        furi_string_set_str(
            error, "Not enough RAM is available to start unlocking this database.");
        return false;
    }

    fp_open_acquire_progress(&ctx, "Reading Header", "", 3U);
    if(!kdbx_parser_process_file(ctx.parser, request->file_path)) {
        furi_string_set_str(error, "Failed to read the database header.");
        fp_open_acquire_log(&ctx, "HEADER_FAIL");
        goto cleanup;
    }

    header = kdbx_parser_get_header(ctx.parser);
    if(header == NULL) {
        furi_string_set_str(error, "Failed to read the database header.");
        fp_open_acquire_log(&ctx, "HEADER_FAIL");
        goto cleanup;
    }

    out_profile->version_minor = header->version_minor;
    out_profile->version_major = header->version_major;
    memcpy(
        out_profile->encryption_algorithm_uuid,
        header->encryption_algorithm_uuid,
        sizeof(out_profile->encryption_algorithm_uuid));
    out_profile->compression_algorithm = header->compression_algorithm;
    memcpy(out_profile->encryption_iv, header->encryption_iv, sizeof(out_profile->encryption_iv));
    out_profile->encryption_iv_size = header->encryption_iv_size;
    out_profile->payload_data_offset = (uint32_t)kdbx_parser_get_payload_offset(ctx.parser);
    if(!kdbx_parser_get_aes_kdf_rounds(ctx.parser, &out_profile->kdf_rounds)) {
        const char* kdf_error = kdbx_parser_get_last_error(ctx.parser);
        furi_string_set_str(
            error,
            (kdf_error != NULL && kdf_error[0] != '\0') ? kdf_error :
                                                          "Unable to inspect the AES-KDF rounds.");
        fp_open_acquire_log(&ctx, "KEY_DERIVE_FAIL");
        goto cleanup;
    }

    {
        char profile_error[128] = {0};
        if(!kdbx_open_profile_validate(out_profile, profile_error, sizeof(profile_error))) {
            furi_string_set_str(error, profile_error);
            fp_open_acquire_log(&ctx, "HEADER_FAIL");
            goto cleanup;
        }
    }

    fp_open_acquire_log(&ctx, "HEADER_OK");
    fp_open_acquire_progress(&ctx, "Key Derivation", "", 10U);
    kdbx_parser_set_kdf_progress_callback(ctx.parser, fp_open_acquire_kdf_progress, &ctx);
    if(!kdbx_parser_derive_key(
           ctx.parser,
           request->password,
           out_profile->cipher_key,
           sizeof(out_profile->cipher_key),
           out_profile->hmac_key,
           sizeof(out_profile->hmac_key))) {
        const char* kdf_error = kdbx_parser_get_last_error(ctx.parser);
        furi_string_set_str(
            error,
            (kdf_error != NULL && kdf_error[0] != '\0') ?
                kdf_error :
                "This database uses an unsupported or invalid KDF.");
        fp_open_acquire_log(&ctx, "KEY_DERIVE_FAIL");
        goto cleanup;
    }

    kdbx_parser_set_kdf_progress_callback(ctx.parser, NULL, NULL);
    {
        uint8_t password_hash[32];
        sha256_Raw((const uint8_t*)request->password, strlen(request->password), password_hash);
        sha256_Raw(password_hash, sizeof(password_hash), out_profile->composite_key);
        out_profile->composite_key_ready = true;
        memzero(password_hash, sizeof(password_hash));
    }
    fp_open_acquire_log(&ctx, "KEY_DERIVE_OK");
    kdbx_parser_free(ctx.parser);
    return true;

cleanup:
    kdbx_parser_set_kdf_progress_callback(ctx.parser, NULL, NULL);
    memzero(out_profile, sizeof(*out_profile));
    kdbx_parser_free(ctx.parser);
    return false;
}

static const FlipPassOpenAcquirePluginV1 flippass_open_acquire_plugin = {
    .api_version = FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION,
    .run = fp_open_acquire_run,
};

static const FlipperAppPluginDescriptor flippass_open_acquire_descriptor = {
    .appid = FLIPPASS_OPEN_ACQUIRE_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION,
    .entry_point = (void*)&flippass_open_acquire_plugin,
};

const FlipperAppPluginDescriptor* flippass_open_acquire_plugin_ep(void) {
    return &flippass_open_acquire_descriptor;
}
