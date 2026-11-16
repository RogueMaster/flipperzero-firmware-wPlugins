#include "aes_cmac.h"

#include "dfc_build_config.h"

#if DFC_ENABLE_AES_CRYPTO
#include "cmac_internal.h"
#include "cmac_common.h"

#define BLOCK_SIZE 16

#define TAG "AESCMAC"

static void aes_cmac_wipe(void* value, size_t length) {
    volatile uint8_t* p = value;
    while(length--)
        *p++ = 0;
}

static void aes_cmac_padBlock(uint8_t* block, size_t len) {
    block[len] = 0x80;
}

static void aes_cmac_bitShiftLeft(uint8_t* input, uint8_t* output, size_t len) {
    size_t last = len - 1;
    for(size_t i = 0; i < last; i++) {
        output[i] = input[i] << 1;
        if(input[i + 1] & 0x80) {
            output[i] += 0x01;
        }
    }
    output[last] = input[last] << 1;
}

static bool
    aes_cmac_generateSubkeys(DfcCryptoAesBlock* cipher, uint8_t* subkey1, uint8_t* subkey2) {
    uint8_t l[BLOCK_SIZE] = {0};
    if(!dfc_crypto_aes_block_encrypt(cipher, l, l)) {
        aes_cmac_wipe(l, sizeof(l));
        return false;
    }

    aes_cmac_bitShiftLeft(l, subkey1, BLOCK_SIZE);
    if(l[0] & 0x80) {
        subkey1[BLOCK_SIZE - 1] ^= 0x87;
    }

    aes_cmac_bitShiftLeft(subkey1, subkey2, BLOCK_SIZE);
    if(subkey1[0] & 0x80) {
        subkey2[BLOCK_SIZE - 1] ^= 0x87;
    }

    aes_cmac_wipe(l, sizeof(l));
    return true;
}

bool dfc_aes_cmac_context_init(DfcAesCmacContext* context, const uint8_t* key, size_t key_len) {
    if(!context) return false;
    memset(context, 0, sizeof(*context));
    if(!dfc_crypto_aes_block_init(&context->cipher, key, key_len)) return false;
    if(!aes_cmac_generateSubkeys(&context->cipher, context->subkey1, context->subkey2)) {
        dfc_aes_cmac_context_clear(context);
        return false;
    }
    context->initialized = true;
    return true;
}

void dfc_aes_cmac_context_clear(DfcAesCmacContext* context) {
    if(!context) return;
    aes_cmac_wipe(context->subkey1, sizeof(context->subkey1));
    aes_cmac_wipe(context->subkey2, sizeof(context->subkey2));
    dfc_crypto_aes_block_clear(&context->cipher);
    aes_cmac_wipe(context, sizeof(*context));
}

bool aes_cmac_with_iv(
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* iv,
    uint8_t* cmac) {
    DfcCryptoSegment segment = {.data = message, .length = message_len};
    return aes_cmac_segments_with_iv(key, key_len, &segment, 1, iv, cmac);
}

bool aes_cmac_segments_with_iv(
    uint8_t* key,
    size_t key_len,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    uint8_t* iv,
    uint8_t* cmac) {
    DfcAesCmacContext context;
    if(!dfc_aes_cmac_context_init(&context, key, key_len)) {
        if(cmac) memset(cmac, 0, BLOCK_SIZE);
        return false;
    }
    bool success = dfc_aes_cmac_context_compute(
        &context, segments, segment_count, iv, DfcCmacFinalStandard, cmac);
    dfc_aes_cmac_context_clear(&context);
    return success;
}

bool dfc_aes_cmac_context_compute(
    DfcAesCmacContext* context,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    const uint8_t iv[BLOCK_SIZE],
    DfcCmacFinalMode final_mode,
    uint8_t cmac[BLOCK_SIZE]) {
    uint8_t pending[BLOCK_SIZE] = {0};
    size_t pending_len = 0;
    size_t total_len = 0;
    uint8_t x[BLOCK_SIZE] = {0};
    bool success = false;

    if(!context || !context->initialized || !iv || !cmac || (segment_count && !segments) ||
       (final_mode != DfcCmacFinalStandard && final_mode != DfcCmacFinalPrePadded)) {
        if(cmac) memset(cmac, 0, BLOCK_SIZE);
        return false;
    }
    for(size_t i = 0; i < segment_count; i++) {
        if((segments[i].length && !segments[i].data) ||
           segments[i].length > SIZE_MAX - total_len) {
            memset(cmac, 0, BLOCK_SIZE);
            return false;
        }
        total_len += segments[i].length;
    }
    if(total_len > SIZE_MAX - (BLOCK_SIZE - 1)) {
        memset(cmac, 0, BLOCK_SIZE);
        return false;
    }
    memcpy(x, iv, sizeof(x));
    for(size_t segment_index = 0; segment_index < segment_count; segment_index++) {
        const uint8_t* data = segments[segment_index].data;
        size_t remaining = segments[segment_index].length;
        while(remaining) {
            if(pending_len == 0 && remaining > BLOCK_SIZE) {
                dfc_cmac_xor_in_place(x, data, BLOCK_SIZE);
                if(!dfc_crypto_aes_block_encrypt(&context->cipher, x, x)) goto cleanup;
                data += BLOCK_SIZE;
                remaining -= BLOCK_SIZE;
                continue;
            }
            if(pending_len == BLOCK_SIZE) {
                dfc_cmac_xor_in_place(x, pending, BLOCK_SIZE);
                if(!dfc_crypto_aes_block_encrypt(&context->cipher, x, x)) goto cleanup;
                pending_len = 0;
            }
            size_t take = BLOCK_SIZE - pending_len;
            if(take > remaining) take = remaining;
            memcpy(pending + pending_len, data, take);
            pending_len += take;
            data += take;
            remaining -= take;
        }
    }
    if(final_mode == DfcCmacFinalPrePadded) {
        if(!total_len || pending_len != BLOCK_SIZE) goto cleanup;
        dfc_cmac_xor_in_place(pending, context->subkey2, BLOCK_SIZE);
    } else if(total_len && pending_len == BLOCK_SIZE) {
        dfc_cmac_xor_in_place(pending, context->subkey1, BLOCK_SIZE);
    } else {
        memset(pending + pending_len, 0, BLOCK_SIZE - pending_len);
        aes_cmac_padBlock(pending, pending_len);
        dfc_cmac_xor_in_place(pending, context->subkey2, BLOCK_SIZE);
    }
    dfc_cmac_xor_in_place(x, pending, BLOCK_SIZE);
    success = dfc_crypto_aes_block_encrypt(&context->cipher, x, cmac);
cleanup:
    if(!success) memset(cmac, 0, BLOCK_SIZE);
    aes_cmac_wipe(pending, sizeof(pending));
    aes_cmac_wipe(x, sizeof(x));
    return success;
}

bool aes_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac) {
    uint8_t iv[BLOCK_SIZE] = {0};
    return aes_cmac_with_iv(key, key_len, message, message_len, iv, cmac);
}

#endif
