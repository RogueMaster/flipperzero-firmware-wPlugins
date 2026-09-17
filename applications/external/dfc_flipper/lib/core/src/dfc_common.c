#include "dfc_common.h"

#include <stdio.h>


const uint8_t DFC_ISO_AID[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};

size_t dfc_key_len_for_cipher(uint8_t cipher) {
    if(cipher == DFC_CMD_AUTHENTICATE_AES) return 16;
    return 8;
}

size_t dfc_block_size_for_cipher(uint8_t cipher) {
    return cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
}

bool dfc_desfire_uid_is_detectable(const uint8_t* uid, size_t uid_len) {
    return (uid_len == DFC_DESFIRE_UID_LEN || uid_len == DFC_DESFIRE_UID_MAX_LENGTH) &&
           uid[0] == DFC_DESFIRE_UID_FIRST_BYTE;
}

const char* dfc_authentication_mode_to_string(uint8_t auth_command) {
    switch(auth_command) {
    case DFC_CMD_AUTHENTICATE_LEGACY:
        return "D40";
    case DFC_CMD_AUTHENTICATE_ISO:
        return "ISO";
    case DFC_CMD_AUTHENTICATE_AES:
        return "AES";
    default:
        return "D40";
    }
}

bool dfc_authentication_mode_from_string(const char* value, uint8_t* auth_command) {
    if(strcmp(value, "D40") == 0 || strcmp(value, "Legacy") == 0 || strcmp(value, "NativeD40") == 0 || strcmp(value, "Native-D40") == 0) {
        *auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
        return true;
    }
    if(strcmp(value, "ISO") == 0) {
        *auth_command = DFC_CMD_AUTHENTICATE_ISO;
        return true;
    }
    if(strcmp(value, "AES") == 0) {
        *auth_command = DFC_CMD_AUTHENTICATE_AES;
        return true;
    }
    return false;
}

bool dfc_authentication_mode_matches_key_settings(uint8_t auth_command, uint8_t key_settings_2) {
    switch(key_settings_2 & DFC_KEY_TYPE_MASK) {
    case DFC_KEY_TYPE_AES:
        return auth_command == DFC_CMD_AUTHENTICATE_AES;
    case DFC_KEY_TYPE_3K3DES:
        return auth_command == DFC_CMD_AUTHENTICATE_ISO;
    case DFC_KEY_TYPE_DES_2K3DES:
    default:
        return auth_command == DFC_CMD_AUTHENTICATE_LEGACY ||
               auth_command == DFC_CMD_AUTHENTICATE_ISO;
    }
}

void dfc_encode_standard_data_file_settings(
    uint8_t file_type,
    uint8_t communication_settings,
    uint16_t access_rights,
    size_t file_size,
    uint8_t output[7]) {
    output[0] = file_type;
    output[1] = communication_settings;
    // `access_rights` is the logical value (read:write:read&write:change from the
    // top nibble); the wire carries it least-significant octet first.
    output[2] = (uint8_t)(access_rights & 0xFF);
    output[3] = (uint8_t)(access_rights >> 8);
    output[4] = (uint8_t)(file_size & 0xFF);
    output[5] = (uint8_t)((file_size >> 8) & 0xFF);
    output[6] = (uint8_t)((file_size >> 16) & 0xFF);
}

void dfc_build_select_application_frame(
    const uint8_t aid[3],
    uint8_t output[DFC_SELECT_APPLICATION_FRAME_SIZE]) {
    output[0] = DFC_CMD_SELECT_APPLICATION;
    output[1] = aid[2];
    output[2] = aid[1];
    output[3] = aid[0];
}

void dfc_build_read_data_frame(
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    uint8_t output[DFC_READ_DATA_FRAME_SIZE]) {
    output[0] = DFC_CMD_READ_DATA;
    output[1] = file_number;
    output[2] = (uint8_t)(offset & 0xFF);
    output[3] = (uint8_t)((offset >> 8) & 0xFF);
    output[4] = (uint8_t)((offset >> 16) & 0xFF);
    output[5] = (uint8_t)(length & 0xFF);
    output[6] = (uint8_t)((length >> 8) & 0xFF);
    output[7] = (uint8_t)((length >> 16) & 0xFF);
}

void dfc_build_wrapped_get_version_frame(uint8_t output[DFC_WRAPPED_GET_VERSION_FRAME_SIZE]) {
    output[0] = DFC_ISO7816_CLA_WRAPPER;
    output[1] = DFC_CMD_GET_VERSION;
    output[2] = 0x00;
    output[3] = 0x00;
    output[4] = 0x00;
}

bool dfc_wrap_native_response_as_iso7816(
    const uint8_t* native_response,
    size_t native_response_len,
    size_t prefix_len,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_len) {
    if(native_response_len <= prefix_len) return false;

    uint8_t native_status = native_response[prefix_len];
    size_t native_data_len = native_response_len - prefix_len - 1;
    size_t required_len = prefix_len + native_data_len + 2;
    if(required_len > output_capacity) return false;

    size_t offset = 0;
    if(prefix_len > 0) {
        memcpy(output, native_response, prefix_len);
        offset += prefix_len;
    }

    if(native_data_len > 0) {
        memcpy(output + offset, native_response + prefix_len + 1, native_data_len);
        offset += native_data_len;
    }

    output[offset++] = 0x91;
    output[offset++] = native_status;
    *output_len = offset;
    return true;
}

void dfc_log_buffer(char* TAG, char* prefix, uint8_t* buffer, size_t buffer_len) {
    (void)TAG;
    char display[DFC_WORKER_MAX_BUFFER_SIZE * 2 + 1];

    size_t limit = DFC_MIN((size_t)DFC_WORKER_MAX_BUFFER_SIZE, buffer_len);
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < limit; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", buffer[i]);
    }
    if(prefix) {
        DFC_LOG_T(TAG, "%s %d: %s", prefix, limit, display);
    } else {
        DFC_LOG_T(TAG, "Buffer %d: %s", limit, display);
    }
}


void dfc_rotate_left(uint8_t* buffer, size_t len) {
    if(len == 0) return;
    uint8_t first = buffer[0];
    memmove(buffer, buffer + 1, len - 1);
    buffer[len - 1] = first;
}

void dfc_worker_des_cbc_decrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    DFC_ASSERT(dfc_crypto_des_cbc(false, key, key_len, iv, encrypted, clear, length));
}

void dfc_worker_des_cbc_encrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    DFC_ASSERT(dfc_crypto_des_cbc(true, key, key_len, iv, clear, encrypted, length));
}

void dfc_worker_aes_cbc_decrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    DFC_ASSERT(dfc_crypto_aes_cbc(false, key, key_len, iv, encrypted, clear, length));
}

void dfc_worker_aes_cbc_encrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    DFC_ASSERT(dfc_crypto_aes_cbc(true, key, key_len, iv, clear, encrypted, length));
}

// True when a 16-octet key is the same 8-octet key twice, which makes it single
// DES rather than 2K3DES.
static bool dfc_key_is_degenerate_des(const uint8_t* key, size_t key_len) {
    return key != NULL && key_len == 16 && memcmp(key, key + 8, 8) == 0;
}

void dfc_derive_session_key(
    uint8_t cipher,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* rnd_a,
    const uint8_t* rnd_b,
    uint8_t* session_key,
    size_t* out_len) {
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        // AES = RndA[0:4] || RndB[0:4] || RndA[12:16] || RndB[12:16]
        memcpy(session_key + 0, rnd_a + 0, 4);
        memcpy(session_key + 4, rnd_b + 0, 4);
        memcpy(session_key + 8, rnd_a + 12, 4);
        memcpy(session_key + 12, rnd_b + 12, 4);
        *out_len = 16;
    } else if(key_len == 24) {
        // 3K3DES = RndA[0:4]||RndB[0:4]||RndA[6:10]||RndB[6:10]||RndA[12:16]||RndB[12:16]
        memcpy(session_key + 0, rnd_a + 0, 4);
        memcpy(session_key + 4, rnd_b + 0, 4);
        memcpy(session_key + 8, rnd_a + 6, 4);
        memcpy(session_key + 12, rnd_b + 6, 4);
        memcpy(session_key + 16, rnd_a + 12, 4);
        memcpy(session_key + 20, rnd_b + 12, 4);
        *out_len = 24;
    } else if(key_len == 16 && !dfc_key_is_degenerate_des(key, key_len)) {
        // 2K3DES = RndA[0:4]||RndB[0:4]||RndA[4:8]||RndB[4:8]
        memcpy(session_key + 0, rnd_a + 0, 4);
        memcpy(session_key + 4, rnd_b + 0, 4);
        memcpy(session_key + 8, rnd_a + 4, 4);
        memcpy(session_key + 12, rnd_b + 4, 4);
        *out_len = 16;
    } else {
        // Single DES = RndA[0:4] || RndB[0:4]. Reached by an 8-octet key and by a
        // 16-octet key whose halves are equal.
        memcpy(session_key + 0, rnd_a + 0, 4);
        memcpy(session_key + 4, rnd_b + 0, 4);
        *out_len = 8;
    }
}
