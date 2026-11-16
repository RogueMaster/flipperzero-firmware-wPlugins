#include "dfc_crypto_internal.h"
#include "dfc_common.h"

#include <string.h>

#if DFC_ENABLE_DES_CRYPTO

bool dfc_crypto_des_chain(
    bool primitive_encrypt,
    DfcCryptoDesChainDirection direction,
    const uint8_t* key,
    size_t key_len,
    const uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    uint8_t previous[8] = {0};
    uint8_t block[8] = {0};
    DfcCryptoDesBlock cipher = {0};
    bool ok = false;

    if(!key || !iv || !input || !output || length % sizeof(block) != 0) goto cleanup;
    memcpy(previous, iv, sizeof(previous));
    if(!dfc_crypto_des_block_init(&cipher, primitive_encrypt, key, key_len)) goto cleanup;
    ok = true;

    for(size_t offset = 0; offset < length; offset += sizeof(block)) {
        if(direction == DfcCryptoDesChainForward) {
            for(size_t i = 0; i < sizeof(block); i++)
                block[i] = input[offset + i] ^ previous[i];
            ok = dfc_crypto_des_block_crypt(&cipher, block, output + offset);
            if(ok) memcpy(previous, output + offset, sizeof(previous));
        } else {
            memcpy(block, input + offset, sizeof(block));
            ok = dfc_crypto_des_block_crypt(&cipher, block, output + offset);
            if(ok) {
                for(size_t i = 0; i < sizeof(block); i++)
                    output[offset + i] ^= previous[i];
                memcpy(previous, block, sizeof(previous));
            }
        }
        if(!ok) break;
    }

cleanup:
    if(!ok && output) dfc_secure_wipe(output, length);
    dfc_crypto_des_block_clear(&cipher);
    dfc_secure_wipe(previous, sizeof(previous));
    dfc_secure_wipe(block, sizeof(block));
    return ok;
}

#endif
