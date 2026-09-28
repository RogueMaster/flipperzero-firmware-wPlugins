#include "dfc_ev2_crypto.h"

#include <string.h>

#if DFC_ENABLE_EV2_SECURE_MESSAGING

void dfc_ev2_rotate_left(
    const uint8_t input[DFC_EV2_RANDOM_LENGTH],
    uint8_t output[DFC_EV2_RANDOM_LENGTH]) {
    memcpy(output, input + 1, DFC_EV2_RANDOM_LENGTH - 1);
    output[DFC_EV2_RANDOM_LENGTH - 1] = input[0];
}

static void build_session_vector(
    uint8_t label_high,
    uint8_t label_low,
    const uint8_t random_a[DFC_EV2_RANDOM_LENGTH],
    const uint8_t random_b[DFC_EV2_RANDOM_LENGTH],
    uint8_t vector[DFC_EV2_SESSION_VECTOR_LENGTH]) {
    memset(vector, 0, DFC_EV2_SESSION_VECTOR_LENGTH);
    vector[0] = label_high;
    vector[1] = label_low;
    vector[2] = DFC_EV2_DERIVATION_COUNTER_HIGH;
    vector[3] = DFC_EV2_DERIVATION_COUNTER_LOW;
    vector[4] = DFC_EV2_DERIVATION_LENGTH_HIGH;
    vector[5] = DFC_EV2_DERIVATION_LENGTH_LOW;
    memcpy(vector + 6, random_a, 2);
    for(size_t i = 0; i < 6; i++) vector[8 + i] = random_a[2 + i] ^ random_b[i];
    memcpy(vector + 14, random_b + 6, 10);
    memcpy(vector + 24, random_a + 8, 8);
}

bool dfc_ev2_derive_session_keys(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t random_a[DFC_EV2_RANDOM_LENGTH],
    const uint8_t random_b[DFC_EV2_RANDOM_LENGTH],
    uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    uint8_t mac_key[DFC_AES_KEY_LENGTH]) {
    uint8_t vector[DFC_EV2_SESSION_VECTOR_LENGTH];
    build_session_vector(
        DFC_EV2_ENCRYPTION_LABEL_HIGH, DFC_EV2_ENCRYPTION_LABEL_LOW, random_a, random_b, vector);
    if(!aes_cmac((uint8_t*)key, DFC_AES_KEY_LENGTH, vector, sizeof(vector), encryption_key))
        return false;
    build_session_vector(
        DFC_EV2_MAC_LABEL_HIGH, DFC_EV2_MAC_LABEL_LOW, random_a, random_b, vector);
    return aes_cmac((uint8_t*)key, DFC_AES_KEY_LENGTH, vector, sizeof(vector), mac_key);
}

bool dfc_ev2_wire_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t* input,
    size_t input_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    uint8_t full[DFC_AES_CMAC_LENGTH];
    if(!aes_cmac((uint8_t*)key, DFC_AES_KEY_LENGTH, (uint8_t*)input, input_len, full))
        return false;
    for(size_t i = 0; i < DFC_WIRE_MAC_LENGTH; i++) output[i] = full[i * 2 + 1];
    return true;
}

bool dfc_ev2_mac(
    const uint8_t mac_key[DFC_AES_KEY_LENGTH],
    uint8_t lead,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    const uint8_t* first,
    size_t first_len,
    const uint8_t* second,
    size_t second_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    uint8_t input[DFC_EV2_MAC_PREFIX_LENGTH + DFC_EV2_MAC_DATA_MAX];
    if(first_len > DFC_EV2_MAC_DATA_MAX || second_len > DFC_EV2_MAC_DATA_MAX - first_len)
        return false;
    input[0] = lead;
    input[1] = (uint8_t)counter;
    input[2] = (uint8_t)(counter >> 8);
    memcpy(input + 3, transaction_identifier, DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
    size_t len = DFC_EV2_MAC_PREFIX_LENGTH;
    if(first_len) memcpy(input + len, first, first_len);
    len += first_len;
    if(second_len) memcpy(input + len, second, second_len);
    len += second_len;
    return dfc_ev2_wire_mac(mac_key, input, len, output);
}

void dfc_ev2_derive_iv(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    uint8_t iv[DFC_AES_KEY_LENGTH]) {
    uint8_t input[DFC_AES_KEY_LENGTH] = {0};
    uint8_t zero_iv[DFC_AES_KEY_LENGTH] = {0};
    if(direction == DfcEv2DirectionCommand) {
        input[0] = DFC_EV2_ENCRYPTION_LABEL_HIGH;
        input[1] = DFC_EV2_ENCRYPTION_LABEL_LOW;
    } else {
        input[0] = DFC_EV2_MAC_LABEL_HIGH;
        input[1] = DFC_EV2_MAC_LABEL_LOW;
    }
    memcpy(input + 2, transaction_identifier, DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
    input[6] = (uint8_t)counter;
    input[7] = (uint8_t)(counter >> 8);
    dfc_worker_aes_cbc_encrypt(
        encryption_key, DFC_AES_KEY_LENGTH, zero_iv, sizeof(input), input, iv);
}

bool dfc_ev2_encrypt_data(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    const uint8_t* clear,
    size_t clear_len,
    uint8_t* encrypted,
    size_t encrypted_capacity,
    size_t* encrypted_len) {
    size_t padded_len = ((clear_len / DFC_AES_KEY_LENGTH) + 1) * DFC_AES_KEY_LENGTH;
    uint8_t padded[DFC_EV2_MAC_DATA_MAX];
    if(padded_len > encrypted_capacity || padded_len > sizeof(padded)) return false;
    memset(padded, 0, padded_len);
    if(clear_len) memcpy(padded, clear, clear_len);
    padded[clear_len] = DFC_EV2_PADDING_MARKER;
    uint8_t iv[DFC_AES_KEY_LENGTH];
    dfc_ev2_derive_iv(encryption_key, direction, counter, transaction_identifier, iv);
    dfc_worker_aes_cbc_encrypt(
        encryption_key, DFC_AES_KEY_LENGTH, iv, padded_len, padded, encrypted);
    *encrypted_len = padded_len;
    return true;
}

bool dfc_ev2_decrypt_data(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* clear,
    size_t* clear_len,
    bool allow_unpadded) {
    if(encrypted_len == 0 || encrypted_len % DFC_AES_KEY_LENGTH != 0) return false;
    uint8_t iv[DFC_AES_KEY_LENGTH];
    dfc_ev2_derive_iv(encryption_key, direction, counter, transaction_identifier, iv);
    dfc_worker_aes_cbc_decrypt(
        encryption_key, DFC_AES_KEY_LENGTH, iv, encrypted_len, encrypted, clear);
    size_t marker = encrypted_len;
    while(marker > 0 && clear[marker - 1] == 0) marker--;
    if(marker == 0 || clear[marker - 1] != DFC_EV2_PADDING_MARKER) {
        if(!allow_unpadded) return false;
        *clear_len = encrypted_len;
        return true;
    }
    *clear_len = marker - 1;
    return true;
}

bool dfc_ev2_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t difference = 0;
    for(size_t i = 0; i < len; i++) difference |= a[i] ^ b[i];
    return difference == 0;
}

#endif // DFC_ENABLE_EV2_SECURE_MESSAGING
