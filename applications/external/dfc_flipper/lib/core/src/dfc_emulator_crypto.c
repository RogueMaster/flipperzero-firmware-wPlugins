#include "dfc_emulator_i.h"
#include "dfc_crypto_internal.h"

#include <string.h>

#if DFC_ENABLE_EMULATOR

uint32_t dfc_emulator_read_uint24_le(const uint8_t* data) {
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || defined(_WIN32)
    uint16_t low;
    memcpy(&low, data, sizeof(low));
    return (uint32_t)low | ((uint32_t)data[2] << 16);
#else
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
#endif
}

bool dfc_emulator_d40_receive_plain(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* plain) {
    static const uint8_t zero_iv[8] = {0};
    return dfc_crypto_des_chain(
        true, DfcCryptoDesChainReverse, key, key_len, zero_iv, encrypted, plain, encrypted_len);
}

uint8_t dfc_emulator_des_key_version(const uint8_t* key) {
    uint8_t version = 0;
    for(size_t i = 0; i < 8; i++) {
        version |= (key[i] & 0x01) << (7 - i);
    }
    return version;
}

#endif // DFC_ENABLE_EMULATOR
