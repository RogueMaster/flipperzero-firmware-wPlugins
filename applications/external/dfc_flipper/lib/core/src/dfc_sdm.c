#include "dfc_sdm.h"

#include <string.h>

#include "aes_cmac.h"
#include "dfc_build_config.h"
#include "dfc_common.h"
#include "dfc_sdm_i.h"

#define SDM_ASCII_OCTETS_PER_BYTE 2
#define SDM_WIRE_MAC_LENGTH       8
#define SDM_ASCII_MAC_LENGTH      (SDM_WIRE_MAC_LENGTH * SDM_ASCII_OCTETS_PER_BYTE)
#define SDM_COUNTER_LENGTH        3
#define SDM_PICC_TAG_UID_MASK     0x0f
#define SDM_PICC_TAG_COUNTER      0x40
#define SDM_PICC_TAG_UID          0x80
#define SDM_PICC_TAG_RESERVED     0x30
#define SDM_MAC_LABEL_HIGH        0x3c
#define SDM_MAC_LABEL_LOW         0xc3
#define SDM_ENC_LABEL_HIGH        0xc3
#define SDM_ENC_LABEL_LOW         0x3c

#if DFC_ENABLE_SDM
static bool range_fits(size_t total, size_t offset, size_t length) {
    return offset <= total && length <= total - offset;
}

static bool
    buffers_overlap(const void* left, size_t left_length, const void* right, size_t right_length) {
    if(!left || !right || left_length == 0 || right_length == 0) return false;
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    if(left_start > UINTPTR_MAX - left_length || right_start > UINTPTR_MAX - right_length)
        return true;
    return left_start < right_start + right_length && right_start < left_start + left_length;
}

static bool hex_nibble(uint8_t value, uint8_t* nibble) {
    if(value >= '0' && value <= '9') {
        *nibble = (uint8_t)(value - '0');
        return true;
    }
    if(value >= 'A' && value <= 'F') {
        *nibble = (uint8_t)(value - 'A' + 10);
        return true;
    }
    if(value >= 'a' && value <= 'f') {
        *nibble = (uint8_t)(value - 'a' + 10);
        return true;
    }
    return false;
}

static bool decode_hex(const uint8_t* encoded, size_t encoded_length, uint8_t* output) {
    if((encoded_length & 1u) != 0) return false;
    for(size_t i = 0; i < encoded_length / 2; i++) {
        uint8_t high = 0;
        uint8_t low = 0;
        if(!hex_nibble(encoded[i * 2], &high) || !hex_nibble(encoded[(i * 2) + 1], &low))
            return false;
        output[i] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static bool valid_hex(const uint8_t* encoded, size_t encoded_length) {
    uint8_t ignored = 0;
    for(size_t index = 0; index < encoded_length; index++) {
        if(!hex_nibble(encoded[index], &ignored)) return false;
    }
    return true;
}
#endif

bool dfc_sdm_derive_session_key(
    const uint8_t static_key[16],
    uint8_t label_high,
    uint8_t label_low,
    const uint8_t* uid,
    size_t uid_length,
    bool include_uid,
    uint32_t read_counter,
    bool include_read_counter,
    uint8_t output[16]) {
    uint8_t vector[32] = {0};
    size_t length = 6;
    bool valid = static_key && output &&
                 (!include_uid || (uid && (uid_length == 7 || uid_length == 10)));
    if(!valid) {
        if(output) dfc_secure_wipe(output, 16);
        return false;
    }
    vector[0] = label_high;
    vector[1] = label_low;
    vector[2] = 0x00;
    vector[3] = 0x01;
    vector[4] = 0x00;
    vector[5] = 0x80;
    if(include_uid) {
        memcpy(vector + length, uid, uid_length);
        length += uid_length;
    }
    if(include_read_counter) {
        vector[length++] = (uint8_t)read_counter;
        vector[length++] = (uint8_t)(read_counter >> 8);
        vector[length++] = (uint8_t)(read_counter >> 16);
    }
    length = ((length + 15) / 16) * 16;
    bool ok = aes_cmac((uint8_t*)static_key, 16, vector, length, output);
    dfc_secure_wipe(vector, sizeof(vector));
    if(!ok) dfc_secure_wipe(output, 16);
    return ok;
}

#if DFC_ENABLE_SDM
static bool config_valid(const DfcSdmVerifyConfig* config, size_t message_length) {
    if(!config || config->mac_input_offset > config->mac_offset ||
       !range_fits(message_length, config->mac_offset, SDM_ASCII_MAC_LENGTH))
        return false;
    if(config->encrypted_picc_data.present) {
        if(config->plain_uid.present || config->plain_uid_length != 0 ||
           config->plain_read_counter.present ||
           (config->encrypted_picc_data_length != 32 &&
            config->encrypted_picc_data_length != 64) ||
           !range_fits(
               message_length,
               config->encrypted_picc_data.offset,
               config->encrypted_picc_data_length))
            return false;
    } else {
        if(config->encrypted_picc_data_length != 0) return false;
        if(config->plain_uid.present &&
           (config->plain_uid_length != 7 && config->plain_uid_length != 10))
            return false;
        if(!config->plain_uid.present && config->plain_uid_length != 0) return false;
        if(config->plain_uid.present && !range_fits(
                                            message_length,
                                            config->plain_uid.offset,
                                            config->plain_uid_length * SDM_ASCII_OCTETS_PER_BYTE))
            return false;
        if(config->plain_read_counter.present &&
           !range_fits(message_length, config->plain_read_counter.offset, 6))
            return false;
    }
    if(config->encrypted_file_data.present) {
        if(config->encrypted_file_data_length == 0 ||
           (config->encrypted_file_data_length % 32) != 0 ||
           !range_fits(
               message_length,
               config->encrypted_file_data.offset,
               config->encrypted_file_data_length) ||
           config->encrypted_file_data.offset < config->mac_input_offset ||
           config->encrypted_file_data.offset > config->mac_offset ||
           config->encrypted_file_data_length >
               config->mac_offset - config->encrypted_file_data.offset)
            return false;
    } else if(config->encrypted_file_data_length != 0) {
        return false;
    }
    return true;
}

static bool
    parse_picc_plaintext(const uint8_t* clear, size_t clear_length, DfcSdmVerifyResult* candidate) {
    uint8_t tag = clear[0];
    size_t uid_length = tag & SDM_PICC_TAG_UID_MASK;
    bool has_uid = (tag & SDM_PICC_TAG_UID) != 0;
    bool has_counter = (tag & SDM_PICC_TAG_COUNTER) != 0;
    if((tag & SDM_PICC_TAG_RESERVED) != 0 || (!has_uid && uid_length != 0) ||
       (has_uid && uid_length != 7 && uid_length != 10) || (!has_uid && !has_counter))
        return false;
    size_t expected_length = has_uid && uid_length == 10 && !has_counter ? 32 : 16;
    if(clear_length != expected_length) return false;
    size_t needed = 1 + uid_length + (has_counter ? SDM_COUNTER_LENGTH : 0);
    if(needed > clear_length) return false;
    size_t uid_offset = uid_length == 10 && !has_counter ? clear_length - uid_length : 1;
    if(has_uid) {
        memcpy(candidate->uid, clear + uid_offset, uid_length);
        candidate->has_uid = true;
        candidate->uid_length = uid_length;
    }
    if(has_counter) {
        size_t counter_offset = 1 + uid_length;
        candidate->read_counter = (uint32_t)clear[counter_offset] |
                                  ((uint32_t)clear[counter_offset + 1] << 8) |
                                  ((uint32_t)clear[counter_offset + 2] << 16);
        candidate->has_read_counter = true;
    }
    return true;
}
#endif

DfcSdmVerifyStatus dfc_sdm_verify_ascii(
    const uint8_t* message,
    size_t message_length,
    const DfcSdmVerifyConfig* config,
    const uint8_t meta_read_key[16],
    const uint8_t file_read_key[16],
    uint8_t* decrypted_file_data,
    size_t decrypted_file_data_capacity,
    size_t* required_decrypted_file_data_length,
    DfcSdmVerifyResult* result) {
#if !DFC_ENABLE_SDM
    (void)message;
    (void)message_length;
    (void)config;
    (void)meta_read_key;
    (void)file_read_key;
    (void)decrypted_file_data;
    (void)decrypted_file_data_capacity;
    if(required_decrypted_file_data_length) *required_decrypted_file_data_length = 0;
    if(result) memset(result, 0, sizeof(*result));
    return DfcSdmVerifyUnsupported;
#else
    DfcSdmVerifyStatus status = DfcSdmVerifyMalformed;
    DfcSdmVerifyResult candidate = {0};
    uint8_t picc_cipher[32] = {0};
    uint8_t picc_clear[32] = {0};
    uint8_t session_mac_key[16] = {0};
    uint8_t full_mac[16] = {0};
    uint8_t expected_mac[SDM_WIRE_MAC_LENGTH] = {0};
    uint8_t received_mac[SDM_WIRE_MAC_LENGTH] = {0};
    uint8_t session_enc_key[16] = {0};
    uint8_t counter_block[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t zero_iv[16] = {0};
    bool output_touched = false;
    size_t decrypted_length =
        config && config->encrypted_file_data.present ? config->encrypted_file_data_length / 2 : 0;
    if(required_decrypted_file_data_length)
        *required_decrypted_file_data_length = decrypted_length;
    if(result) memset(result, 0, sizeof(*result));
    if(!message || !result || !file_read_key || !config_valid(config, message_length))
        goto cleanup;
    if(buffers_overlap(message, message_length, decrypted_file_data, decrypted_file_data_capacity))
        goto cleanup;
    if(!valid_hex(message + config->mac_offset, SDM_ASCII_MAC_LENGTH) ||
       (config->plain_uid.present &&
        !valid_hex(message + config->plain_uid.offset, config->plain_uid_length * 2)) ||
       (config->plain_read_counter.present &&
        !valid_hex(message + config->plain_read_counter.offset, 6)) ||
       (config->encrypted_picc_data.present &&
        !valid_hex(
            message + config->encrypted_picc_data.offset, config->encrypted_picc_data_length)) ||
       (config->encrypted_file_data.present &&
        !valid_hex(
            message + config->encrypted_file_data.offset, config->encrypted_file_data_length)))
        goto cleanup;

    if(config->encrypted_picc_data.present) {
        size_t clear_length = config->encrypted_picc_data_length / 2;
        if(!meta_read_key || !decode_hex(
                                 message + config->encrypted_picc_data.offset,
                                 config->encrypted_picc_data_length,
                                 picc_cipher))
            goto cleanup;
        if(!dfc_worker_aes_cbc_decrypt(
               meta_read_key, 16, zero_iv, clear_length, picc_cipher, picc_clear)) {
            status = DfcSdmVerifyCryptoFailed;
            goto cleanup;
        }
        if(!parse_picc_plaintext(picc_clear, clear_length, &candidate)) {
            // An invalid plaintext is indistinguishable from an incorrect key.
            status = DfcSdmVerifyAuthenticationFailed;
            goto cleanup;
        }
    } else {
        if(config->plain_uid.present) {
            if(!decode_hex(
                   message + config->plain_uid.offset, config->plain_uid_length * 2, candidate.uid))
                goto cleanup;
            candidate.has_uid = true;
            candidate.uid_length = config->plain_uid_length;
        }
        if(config->plain_read_counter.present) {
            uint8_t counter[3] = {0};
            bool decoded = decode_hex(message + config->plain_read_counter.offset, 6, counter);
            if(!decoded) {
                dfc_secure_wipe(counter, sizeof(counter));
                goto cleanup;
            }
            candidate.read_counter = ((uint32_t)counter[0] << 16) | ((uint32_t)counter[1] << 8) |
                                     counter[2];
            candidate.has_read_counter = true;
            dfc_secure_wipe(counter, sizeof(counter));
        }
    }

    if(!dfc_sdm_derive_session_key(
           file_read_key,
           SDM_MAC_LABEL_HIGH,
           SDM_MAC_LABEL_LOW,
           candidate.uid,
           candidate.uid_length,
           candidate.has_uid,
           candidate.read_counter,
           candidate.has_read_counter,
           session_mac_key) ||
       !aes_cmac(
           session_mac_key,
           16,
           (uint8_t*)message + config->mac_input_offset,
           config->mac_offset - config->mac_input_offset,
           full_mac)) {
        status = DfcSdmVerifyCryptoFailed;
        goto cleanup;
    }
    dfc_truncate_aes_wire_mac(full_mac, expected_mac);
    if(!decode_hex(message + config->mac_offset, SDM_ASCII_MAC_LENGTH, received_mac)) goto cleanup;
    if(!dfc_ct_equal(expected_mac, received_mac, sizeof(expected_mac))) {
        status = DfcSdmVerifyAuthenticationFailed;
        goto cleanup;
    }

    if(decrypted_length != 0) {
        if(!candidate.has_uid || !candidate.has_read_counter) {
            status = DfcSdmVerifyUnsupported;
            goto cleanup;
        }
        if(!decrypted_file_data || decrypted_file_data_capacity < decrypted_length) {
            status = DfcSdmVerifyBufferTooSmall;
            goto cleanup;
        }
        if(!decode_hex(
               message + config->encrypted_file_data.offset,
               config->encrypted_file_data_length,
               decrypted_file_data))
            goto cleanup;
        output_touched = true;
        if(!dfc_sdm_derive_session_key(
               file_read_key,
               SDM_ENC_LABEL_HIGH,
               SDM_ENC_LABEL_LOW,
               candidate.uid,
               candidate.uid_length,
               true,
               candidate.read_counter,
               true,
               session_enc_key)) {
            status = DfcSdmVerifyCryptoFailed;
            goto cleanup;
        }
        counter_block[0] = (uint8_t)candidate.read_counter;
        counter_block[1] = (uint8_t)(candidate.read_counter >> 8);
        counter_block[2] = (uint8_t)(candidate.read_counter >> 16);
        if(!dfc_worker_aes_cbc_encrypt(
               session_enc_key, 16, zero_iv, sizeof(counter_block), counter_block, iv) ||
           !dfc_worker_aes_cbc_decrypt(
               session_enc_key,
               16,
               iv,
               decrypted_length,
               decrypted_file_data,
               decrypted_file_data)) {
            dfc_secure_wipe(decrypted_file_data, decrypted_length);
            status = DfcSdmVerifyCryptoFailed;
            goto cleanup;
        }
    }

    candidate.decrypted_file_data_length = decrypted_length;
    *result = candidate;
    status = DfcSdmVerifyOk;

cleanup:
    if(status != DfcSdmVerifyOk && output_touched)
        dfc_secure_wipe(decrypted_file_data, decrypted_length);
    dfc_secure_wipe(picc_cipher, sizeof(picc_cipher));
    dfc_secure_wipe(picc_clear, sizeof(picc_clear));
    dfc_secure_wipe(session_mac_key, sizeof(session_mac_key));
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    dfc_secure_wipe(expected_mac, sizeof(expected_mac));
    dfc_secure_wipe(received_mac, sizeof(received_mac));
    dfc_secure_wipe(session_enc_key, sizeof(session_enc_key));
    dfc_secure_wipe(counter_block, sizeof(counter_block));
    dfc_secure_wipe(iv, sizeof(iv));
    dfc_secure_wipe(zero_iv, sizeof(zero_iv));
    if(status != DfcSdmVerifyOk) dfc_secure_wipe(&candidate, sizeof(candidate));
    return status;
#endif
}
