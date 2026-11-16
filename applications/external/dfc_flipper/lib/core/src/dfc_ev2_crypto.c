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
    for(size_t i = 0; i < 6; i++)
        vector[8 + i] = random_a[2 + i] ^ random_b[i];
    memcpy(vector + 14, random_b + 6, 10);
    memcpy(vector + 24, random_a + 8, 8);
}

bool dfc_ev2_derive_session_keys(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t random_a[DFC_EV2_RANDOM_LENGTH],
    const uint8_t random_b[DFC_EV2_RANDOM_LENGTH],
    uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    uint8_t mac_key[DFC_AES_KEY_LENGTH]) {
    if(!key || !random_a || !random_b || !encryption_key || !mac_key) return false;
    uint8_t vector[DFC_EV2_SESSION_VECTOR_LENGTH] = {0};
    uint8_t candidate_encryption_key[DFC_AES_KEY_LENGTH] = {0};
    uint8_t candidate_mac_key[DFC_AES_KEY_LENGTH] = {0};
    bool success = false;
    build_session_vector(
        DFC_EV2_ENCRYPTION_LABEL_HIGH, DFC_EV2_ENCRYPTION_LABEL_LOW, random_a, random_b, vector);
    if(!aes_cmac(
           (uint8_t*)key, DFC_AES_KEY_LENGTH, vector, sizeof(vector), candidate_encryption_key))
        goto cleanup;
    build_session_vector(
        DFC_EV2_MAC_LABEL_HIGH, DFC_EV2_MAC_LABEL_LOW, random_a, random_b, vector);
    if(!aes_cmac((uint8_t*)key, DFC_AES_KEY_LENGTH, vector, sizeof(vector), candidate_mac_key))
        goto cleanup;
    memcpy(encryption_key, candidate_encryption_key, sizeof(candidate_encryption_key));
    memcpy(mac_key, candidate_mac_key, sizeof(candidate_mac_key));
    success = true;
cleanup:
    if(!success) {
        memset(encryption_key, 0, DFC_AES_KEY_LENGTH);
        memset(mac_key, 0, DFC_AES_KEY_LENGTH);
    }
    dfc_secure_wipe(vector, sizeof(vector));
    dfc_secure_wipe(candidate_encryption_key, sizeof(candidate_encryption_key));
    dfc_secure_wipe(candidate_mac_key, sizeof(candidate_mac_key));
    return success;
}

bool dfc_ev2_wire_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t* input,
    size_t input_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    return dfc_aes_wire_mac(key, input, input_len, output);
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
    if(!output) return false;
    memset(output, 0, DFC_WIRE_MAC_LENGTH);
    if(!mac_key || !transaction_identifier || (first_len != 0 && !first) ||
       (second_len != 0 && !second) || first_len > DFC_EV2_MAC_DATA_MAX ||
       second_len > DFC_EV2_MAC_DATA_MAX - first_len)
        return false;
    uint8_t prefix[DFC_EV2_MAC_PREFIX_LENGTH];
    prefix[0] = lead;
    prefix[1] = (uint8_t)counter;
    prefix[2] = (uint8_t)(counter >> 8);
    memcpy(prefix + 3, transaction_identifier, DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
    DfcCryptoSegment segments[] = {
        {.data = prefix, .length = sizeof(prefix)},
        {.data = first, .length = first_len},
        {.data = second, .length = second_len},
    };
    uint8_t zero_iv[DFC_AES_CMAC_LENGTH] = {0};
    uint8_t full[DFC_AES_CMAC_LENGTH] = {0};
    if(!aes_cmac_segments_with_iv(
           (uint8_t*)mac_key,
           DFC_AES_KEY_LENGTH,
           segments,
           sizeof(segments) / sizeof(segments[0]),
           zero_iv,
           full)) {
        dfc_secure_wipe(full, sizeof(full));
        dfc_secure_wipe(prefix, sizeof(prefix));
        return false;
    }
    dfc_truncate_aes_wire_mac(full, output);
    dfc_secure_wipe(full, sizeof(full));
    dfc_secure_wipe(prefix, sizeof(prefix));
    return true;
}

bool dfc_ev2_derive_iv(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    uint8_t iv[DFC_AES_KEY_LENGTH]) {
    if(!iv) return false;
    memset(iv, 0, DFC_AES_KEY_LENGTH);
    if(!encryption_key || !transaction_identifier) return false;
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
    bool ok = dfc_worker_aes_cbc_encrypt(
        encryption_key, DFC_AES_KEY_LENGTH, zero_iv, sizeof(input), input, iv);
    dfc_secure_wipe(input, sizeof(input));
    dfc_secure_wipe(zero_iv, sizeof(zero_iv));
    if(!ok) memset(iv, 0, DFC_AES_KEY_LENGTH);
    return ok;
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
    if(!encrypted_len) return false;
    *encrypted_len = 0;
    if(!encryption_key || !transaction_identifier || !encrypted || (clear_len && !clear) ||
       clear_len == SIZE_MAX) {
        return false;
    }
    size_t padded_len = clear_len + 1;
    size_t remainder = padded_len % DFC_AES_KEY_LENGTH;
    size_t padding = remainder ? DFC_AES_KEY_LENGTH - remainder : 0;
    if(padded_len > SIZE_MAX - padding) return false;
    padded_len += padding;
    if(padded_len > encrypted_capacity || padded_len > DFC_EV2_MAC_DATA_MAX) return false;

    if(clear_len) memmove(encrypted, clear, clear_len);
    encrypted[clear_len] = DFC_EV2_PADDING_MARKER;
    memset(encrypted + clear_len + 1, 0, padded_len - clear_len - 1);
    uint8_t iv[DFC_AES_KEY_LENGTH];
    if(!dfc_ev2_derive_iv(encryption_key, direction, counter, transaction_identifier, iv) ||
       !dfc_worker_aes_cbc_encrypt(
           encryption_key, DFC_AES_KEY_LENGTH, iv, padded_len, encrypted, encrypted)) {
        dfc_secure_wipe(iv, sizeof(iv));
        dfc_secure_wipe(encrypted, padded_len);
        return false;
    }
    dfc_secure_wipe(iv, sizeof(iv));
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
    if(!clear_len) return false;
    *clear_len = 0;
    if(!encryption_key || !transaction_identifier || !encrypted || !clear || encrypted_len == 0 ||
       encrypted_len > DFC_EV2_MAC_DATA_MAX || encrypted_len % DFC_AES_KEY_LENGTH != 0) {
        if(clear && encrypted_len <= DFC_EV2_MAC_DATA_MAX) dfc_secure_wipe(clear, encrypted_len);
        return false;
    }
    uint8_t iv[DFC_AES_KEY_LENGTH];
    if(!dfc_ev2_derive_iv(encryption_key, direction, counter, transaction_identifier, iv) ||
       !dfc_worker_aes_cbc_decrypt(
           encryption_key, DFC_AES_KEY_LENGTH, iv, encrypted_len, encrypted, clear)) {
        dfc_secure_wipe(iv, sizeof(iv));
        dfc_secure_wipe(clear, encrypted_len);
        return false;
    }
    dfc_secure_wipe(iv, sizeof(iv));
    size_t marker = encrypted_len;
    while(marker > 0 && clear[marker - 1] == 0)
        marker--;
    if(marker == 0 || clear[marker - 1] != DFC_EV2_PADDING_MARKER) {
        if(!allow_unpadded) {
            dfc_secure_wipe(clear, encrypted_len);
            return false;
        }
        *clear_len = encrypted_len;
        return true;
    }
    *clear_len = marker - 1;
    return true;
}

#endif // DFC_ENABLE_EV2_SECURE_MESSAGING
