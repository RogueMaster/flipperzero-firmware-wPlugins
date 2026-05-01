#include "flippass_otp_plugin.h"

#include "../kdbx/hmac.h"
#include "../kdbx/memzero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OTP_HMAC_MAX_SIZE SHA512_DIGEST_LENGTH

typedef struct {
    uint8_t o_key_pad[SHA1_BLOCK_LENGTH];
    SHA1_CTX ctx;
} FlipPassOtpHmacSha1Ctx;

static bool flippass_otp_char_is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static void
    flippass_otp_hmac_sha1_init(FlipPassOtpHmacSha1Ctx* hctx, const uint8_t* key, size_t key_len) {
    uint8_t i_key_pad[SHA1_BLOCK_LENGTH];

    memzero(i_key_pad, sizeof(i_key_pad));
    if(key_len > SHA1_BLOCK_LENGTH) {
        sha1_Raw(key, key_len, i_key_pad);
    } else if(key_len > 0U) {
        memcpy(i_key_pad, key, key_len);
    }

    for(size_t index = 0U; index < SHA1_BLOCK_LENGTH; index++) {
        hctx->o_key_pad[index] = i_key_pad[index] ^ 0x5cU;
        i_key_pad[index] ^= 0x36U;
    }

    sha1_Init(&hctx->ctx);
    sha1_Update(&hctx->ctx, i_key_pad, sizeof(i_key_pad));
    memzero(i_key_pad, sizeof(i_key_pad));
}

static void flippass_otp_hmac_sha1_update(
    FlipPassOtpHmacSha1Ctx* hctx,
    const uint8_t* data,
    size_t data_len) {
    sha1_Update(&hctx->ctx, data, data_len);
}

static void
    flippass_otp_hmac_sha1_final(FlipPassOtpHmacSha1Ctx* hctx, uint8_t out[SHA1_DIGEST_LENGTH]) {
    sha1_Final(&hctx->ctx, out);
    sha1_Init(&hctx->ctx);
    sha1_Update(&hctx->ctx, hctx->o_key_pad, sizeof(hctx->o_key_pad));
    sha1_Update(&hctx->ctx, out, SHA1_DIGEST_LENGTH);
    sha1_Final(&hctx->ctx, out);
    memzero(hctx, sizeof(*hctx));
}

static int8_t flippass_otp_hex_nibble(char ch) {
    if(ch >= '0' && ch <= '9') {
        return (int8_t)(ch - '0');
    }
    if(ch >= 'a' && ch <= 'f') {
        return (int8_t)(ch - 'a' + 10);
    }
    if(ch >= 'A' && ch <= 'F') {
        return (int8_t)(ch - 'A' + 10);
    }
    return -1;
}

static bool flippass_otp_decode_hex(
    const char* text,
    uint8_t** out_bytes,
    size_t* out_size,
    FuriString* error) {
    size_t digits = 0U;
    size_t out_index = 0U;
    int8_t high = -1;

    for(const char* cursor = text; cursor != NULL && *cursor != '\0'; cursor++) {
        if(flippass_otp_char_is_space(*cursor)) {
            continue;
        }
        if(flippass_otp_hex_nibble(*cursor) < 0) {
            furi_string_set_str(error, "The OTP Hex secret contains an invalid character.");
            return false;
        }
        digits++;
    }

    if(digits == 0U || (digits % 2U) != 0U) {
        furi_string_set_str(error, "The OTP Hex secret must contain full bytes.");
        return false;
    }

    uint8_t* bytes = malloc(digits / 2U);
    if(bytes == NULL) {
        furi_string_set_str(error, "Not enough RAM to decode the OTP secret.");
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(flippass_otp_char_is_space(*cursor)) {
            continue;
        }

        const int8_t nibble = flippass_otp_hex_nibble(*cursor);
        if(high < 0) {
            high = nibble;
        } else {
            bytes[out_index++] = (uint8_t)(((uint8_t)high << 4U) | (uint8_t)nibble);
            high = -1;
        }
    }

    *out_bytes = bytes;
    *out_size = out_index;
    return true;
}

static int8_t flippass_otp_base32_value(char ch) {
    if(ch == '0') {
        ch = 'O';
    } else if(ch == '1') {
        ch = 'L';
    } else if(ch == '8') {
        ch = 'B';
    }

    if(ch >= 'A' && ch <= 'Z') {
        return (int8_t)(ch - 'A');
    }
    if(ch >= 'a' && ch <= 'z') {
        return (int8_t)(ch - 'a');
    }
    if(ch >= '2' && ch <= '7') {
        return (int8_t)(ch - '2' + 26);
    }
    return -1;
}

static bool flippass_otp_decode_base32(
    const char* text,
    uint8_t** out_bytes,
    size_t* out_size,
    FuriString* error) {
    size_t symbols = 0U;
    uint32_t buffer = 0U;
    uint8_t bits_left = 0U;
    size_t out_index = 0U;

    for(const char* cursor = text; cursor != NULL && *cursor != '\0'; cursor++) {
        if(flippass_otp_char_is_space(*cursor) || *cursor == '-' || *cursor == '=') {
            continue;
        }
        if(flippass_otp_base32_value(*cursor) < 0) {
            furi_string_set_str(error, "The OTP Base32 secret contains an invalid character.");
            return false;
        }
        symbols++;
    }

    if(symbols == 0U) {
        furi_string_set_str(error, "The OTP Base32 secret is empty.");
        return false;
    }

    uint8_t* bytes = malloc(((symbols * 5U) / 8U) + 1U);
    if(bytes == NULL) {
        furi_string_set_str(error, "Not enough RAM to decode the OTP secret.");
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(flippass_otp_char_is_space(*cursor) || *cursor == '-' || *cursor == '=') {
            continue;
        }

        const int8_t value = flippass_otp_base32_value(*cursor);
        buffer = (buffer << 5U) | (uint32_t)value;
        bits_left = (uint8_t)(bits_left + 5U);
        if(bits_left >= 8U) {
            bytes[out_index++] = (uint8_t)(buffer >> (bits_left - 8U));
            bits_left = (uint8_t)(bits_left - 8U);
        }
    }

    if(out_index == 0U) {
        memzero(bytes, ((symbols * 5U) / 8U) + 1U);
        free(bytes);
        furi_string_set_str(error, "The OTP Base32 secret is too short.");
        return false;
    }

    *out_bytes = bytes;
    *out_size = out_index;
    return true;
}

static int8_t flippass_otp_base64_value(char ch) {
    if(ch >= 'A' && ch <= 'Z') {
        return (int8_t)(ch - 'A');
    }
    if(ch >= 'a' && ch <= 'z') {
        return (int8_t)(ch - 'a' + 26);
    }
    if(ch >= '0' && ch <= '9') {
        return (int8_t)(ch - '0' + 52);
    }
    if(ch == '+') {
        return 62;
    }
    if(ch == '/' || ch == '_') {
        return 63;
    }
    if(ch == '-') {
        return 62;
    }
    return -1;
}

static bool flippass_otp_decode_base64(
    const char* text,
    uint8_t** out_bytes,
    size_t* out_size,
    FuriString* error) {
    size_t symbols = 0U;
    uint32_t buffer = 0U;
    uint8_t bits_left = 0U;
    size_t out_index = 0U;
    bool padding = false;

    for(const char* cursor = text; cursor != NULL && *cursor != '\0'; cursor++) {
        if(flippass_otp_char_is_space(*cursor)) {
            continue;
        }
        if(*cursor == '=') {
            padding = true;
            continue;
        }
        if(padding || flippass_otp_base64_value(*cursor) < 0) {
            furi_string_set_str(error, "The OTP Base64 secret contains an invalid character.");
            return false;
        }
        symbols++;
    }

    if(symbols == 0U) {
        furi_string_set_str(error, "The OTP Base64 secret is empty.");
        return false;
    }

    uint8_t* bytes = malloc(((symbols * 6U) / 8U) + 1U);
    if(bytes == NULL) {
        furi_string_set_str(error, "Not enough RAM to decode the OTP secret.");
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(flippass_otp_char_is_space(*cursor) || *cursor == '=') {
            continue;
        }

        const int8_t value = flippass_otp_base64_value(*cursor);
        buffer = (buffer << 6U) | (uint32_t)value;
        bits_left = (uint8_t)(bits_left + 6U);
        if(bits_left >= 8U) {
            bytes[out_index++] = (uint8_t)(buffer >> (bits_left - 8U));
            bits_left = (uint8_t)(bits_left - 8U);
        }
    }

    if(out_index == 0U) {
        memzero(bytes, ((symbols * 6U) / 8U) + 1U);
        free(bytes);
        furi_string_set_str(error, "The OTP Base64 secret is too short.");
        return false;
    }

    *out_bytes = bytes;
    *out_size = out_index;
    return true;
}

static bool flippass_otp_decode_secret(
    const FlipPassOtpPluginRequestV1* request,
    uint8_t** out_bytes,
    size_t* out_size,
    bool* out_allocated,
    FuriString* error) {
    if(request->secret == NULL || request->secret[0] == '\0') {
        furi_string_set_str(error, "The OTP secret is missing.");
        return false;
    }

    *out_bytes = NULL;
    *out_size = 0U;
    *out_allocated = false;

    switch(request->secret_encoding) {
    case FlipPassOtpSecretEncodingText:
        *out_bytes = (uint8_t*)request->secret;
        *out_size = strlen(request->secret);
        return *out_size > 0U;
    case FlipPassOtpSecretEncodingHex:
        *out_allocated = true;
        return flippass_otp_decode_hex(request->secret, out_bytes, out_size, error);
    case FlipPassOtpSecretEncodingBase32:
        *out_allocated = true;
        return flippass_otp_decode_base32(request->secret, out_bytes, out_size, error);
    case FlipPassOtpSecretEncodingBase64:
        *out_allocated = true;
        return flippass_otp_decode_base64(request->secret, out_bytes, out_size, error);
    default:
        furi_string_set_str(error, "The OTP secret encoding is unsupported.");
        return false;
    }
}

static bool flippass_otp_hmac(
    FlipPassOtpAlgorithm algorithm,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t out[FLIPPASS_OTP_HMAC_MAX_SIZE],
    size_t* out_len,
    FuriString* error) {
    switch(algorithm) {
    case FlipPassOtpAlgorithmSha1: {
        FlipPassOtpHmacSha1Ctx ctx = {0};
        flippass_otp_hmac_sha1_init(&ctx, key, key_len);
        flippass_otp_hmac_sha1_update(&ctx, input, 8U);
        flippass_otp_hmac_sha1_final(&ctx, out);
        *out_len = SHA1_DIGEST_LENGTH;
        return true;
    }
    case FlipPassOtpAlgorithmSha256:
        hmac_sha256(key, (uint32_t)key_len, input, 8U, out);
        *out_len = SHA256_DIGEST_LENGTH;
        return true;
    case FlipPassOtpAlgorithmSha512:
        hmac_sha512(key, (uint32_t)key_len, input, 8U, out);
        *out_len = SHA512_DIGEST_LENGTH;
        return true;
    default:
        furi_string_set_str(error, "The OTP algorithm is unsupported.");
        return false;
    }
}

static uint32_t flippass_otp_pow10(uint8_t digits) {
    uint32_t value = 1U;
    for(uint8_t index = 0U; index < digits; index++) {
        value *= 10U;
    }
    return value;
}

static void flippass_otp_write_be64(uint64_t value, uint8_t out[8]) {
    for(size_t index = 0U; index < 8U; index++) {
        out[7U - index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static bool flippass_otp_plugin_generate(
    const FlipPassOtpPluginRequestV1* request,
    FlipPassOtpPluginResultV1* result,
    FuriString* error) {
    uint8_t* secret = NULL;
    size_t secret_size = 0U;
    bool secret_allocated = false;
    uint8_t input[8];
    uint8_t hmac[FLIPPASS_OTP_HMAC_MAX_SIZE];
    size_t hmac_size = 0U;
    bool ok = false;

    if(request == NULL || result == NULL ||
       request->api_version != FLIPPASS_OTP_PLUGIN_API_VERSION) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass OTP plugin received an invalid request.");
        }
        return false;
    }

    memset(result, 0, sizeof(*result));

    const uint8_t digits = request->digits;
    if(digits == 0U || digits > FLIPPASS_OTP_MAX_DIGITS) {
        if(error != NULL) {
            furi_string_set_str(error, "The OTP digit count is unsupported.");
        }
        return false;
    }

    if(!flippass_otp_decode_secret(request, &secret, &secret_size, &secret_allocated, error)) {
        return false;
    }

    uint64_t input_value = request->counter;
    if(request->kind == FlipPassOtpKindTime) {
        const uint32_t period = request->period > 0U ? request->period :
                                                       FLIPPASS_OTP_DEFAULT_PERIOD;
        int64_t corrected_time = (int64_t)request->unix_time - request->time_zone_offset_seconds;
        if(corrected_time < 0) {
            corrected_time = 0;
        }
        input_value = ((uint64_t)corrected_time) / period;
    } else if(request->kind != FlipPassOtpKindHmac) {
        if(error != NULL) {
            furi_string_set_str(error, "The OTP type is unsupported.");
        }
        goto cleanup;
    }

    flippass_otp_write_be64(input_value, input);
    if(!flippass_otp_hmac(
           request->algorithm, secret, secret_size, input, hmac, &hmac_size, error)) {
        goto cleanup;
    }

    const uint8_t offset = hmac[hmac_size - 1U] & 0x0FU;
    if(offset + 3U >= hmac_size) {
        if(error != NULL) {
            furi_string_set_str(error, "The OTP HMAC output was invalid.");
        }
        goto cleanup;
    }

    const uint32_t binary = (((uint32_t)hmac[offset] & 0x7FU) << 24U) |
                            (((uint32_t)hmac[offset + 1U] & 0xFFU) << 16U) |
                            (((uint32_t)hmac[offset + 2U] & 0xFFU) << 8U) |
                            ((uint32_t)hmac[offset + 3U] & 0xFFU);
    uint32_t code = binary % flippass_otp_pow10(digits);
    result->code[digits] = '\0';
    for(uint8_t pos = digits; pos > 0U; pos--) {
        result->code[pos - 1U] = (char)('0' + (code % 10U));
        code /= 10U;
    }
    result->next_counter = request->counter + 1ULL;
    ok = true;

cleanup:
    memzero(input, sizeof(input));
    memzero(hmac, sizeof(hmac));
    if(secret_allocated && secret != NULL) {
        memzero(secret, secret_size);
        free(secret);
    }
    return ok;
}

static const FlipPassOtpPluginV1 flippass_otp_plugin = {
    .api_version = FLIPPASS_OTP_PLUGIN_API_VERSION,
    .generate = flippass_otp_plugin_generate,
};

static const FlipperAppPluginDescriptor flippass_otp_descriptor = {
    .appid = FLIPPASS_OTP_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OTP_PLUGIN_API_VERSION,
    .entry_point = &flippass_otp_plugin,
};

const FlipperAppPluginDescriptor* flippass_otp_plugin_ep(void) {
    return &flippass_otp_descriptor;
}
