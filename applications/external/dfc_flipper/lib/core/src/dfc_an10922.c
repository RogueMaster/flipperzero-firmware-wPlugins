#include "dfc_an10922.h"

#include <string.h>

#include "cmac_internal.h"

#define AN10922_AES_INPUT_MAX  31
#define AN10922_TDEA_INPUT_MAX 15

static void an10922_wipe(void* value, size_t length) {
    volatile uint8_t* p = value;
    while(length--)
        *p++ = 0;
}

size_t dfc_an10922_key_length(DfcAn10922Algorithm algorithm) {
    switch(algorithm) {
    case DfcAn10922Aes128:
    case DfcAn10922Tdea2:
        return 16;
    case DfcAn10922Tdea3:
        return 24;
    default:
        return 0;
    }
}

static bool diversify_aes128(
    const uint8_t master_key[16],
    const uint8_t* input,
    size_t input_len,
    uint8_t output[16]) {
#if DFC_ENABLE_AES_CRYPTO
    uint8_t message[32] = {0};
    uint8_t iv[16] = {0};
    DfcAesCmacContext context;
    bool padded = input_len < AN10922_AES_INPUT_MAX;

    message[0] = 0x01;
    memcpy(message + 1, input, input_len);
    if(padded) message[input_len + 1] = 0x80;

    bool success = dfc_aes_cmac_context_init(&context, master_key, 16);
    if(success) {
        DfcCryptoSegment segment = {.data = message, .length = sizeof(message)};
        success = dfc_aes_cmac_context_compute(
            &context,
            &segment,
            1,
            iv,
            padded ? DfcCmacFinalPrePadded : DfcCmacFinalStandard,
            output);
        dfc_aes_cmac_context_clear(&context);
    }
    an10922_wipe(message, sizeof(message));
    an10922_wipe(iv, sizeof(iv));
    return success;
#else
    (void)master_key;
    (void)input;
    (void)input_len;
    (void)output;
    return false;
#endif
}

static bool diversify_tdea(
    const uint8_t* master_key,
    size_t key_len,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output) {
#if DFC_ENABLE_DES_CRYPTO
    uint8_t message[16] = {0};
    uint8_t iv[8] = {0};
    DfcDesCmacContext context;
    bool padded = input_len < AN10922_TDEA_INPUT_MAX;
    size_t part_count = key_len / 8;

    memcpy(message + 1, input, input_len);
    if(padded) message[input_len + 1] = 0x80;

    bool success = dfc_des_cmac_context_init(&context, master_key, key_len);
    for(size_t part = 0; success && part < part_count; part++) {
        message[0] = (uint8_t)(key_len == 16 ? 0x21 + part : 0x31 + part);
        DfcCryptoSegment segment = {.data = message, .length = sizeof(message)};
        success = dfc_des_cmac_context_compute(
            &context,
            &segment,
            1,
            iv,
            padded ? DfcCmacFinalPrePadded : DfcCmacFinalStandard,
            output + part * 8);
    }
    if(context.initialized) dfc_des_cmac_context_clear(&context);
    if(success) {
        for(size_t i = 0; i < 8; i++)
            output[i] = (uint8_t)((output[i] & 0xfe) | (master_key[i] & 0x01));
    }
    an10922_wipe(message, sizeof(message));
    an10922_wipe(iv, sizeof(iv));
    return success;
#else
    (void)master_key;
    (void)key_len;
    (void)input;
    (void)input_len;
    (void)output;
    return false;
#endif
}

bool dfc_an10922_diversify(
    DfcAn10922Algorithm algorithm,
    const uint8_t* master_key,
    size_t master_key_len,
    const uint8_t* diversification_input,
    size_t diversification_input_len,
    uint8_t* diversified_key,
    size_t diversified_key_capacity) {
    uint8_t candidate[24] = {0};
    size_t key_len = dfc_an10922_key_length(algorithm);
    size_t input_max = algorithm == DfcAn10922Aes128 ? AN10922_AES_INPUT_MAX :
                                                       AN10922_TDEA_INPUT_MAX;
    bool output_available = diversified_key && key_len && diversified_key_capacity >= key_len;

    if(!key_len || !master_key || master_key_len != key_len || !diversification_input ||
       diversification_input_len == 0 || diversification_input_len > input_max ||
       !output_available) {
        if(output_available) memset(diversified_key, 0, key_len);
        return false;
    }

    bool success =
        algorithm == DfcAn10922Aes128 ?
            diversify_aes128(
                master_key, diversification_input, diversification_input_len, candidate) :
            diversify_tdea(
                master_key, key_len, diversification_input, diversification_input_len, candidate);
    if(success)
        memmove(diversified_key, candidate, key_len);
    else
        memset(diversified_key, 0, key_len);
    an10922_wipe(candidate, sizeof(candidate));
    return success;
}
