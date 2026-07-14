#include "kdbx_open_profile.h"

#include <stdio.h>
#include <string.h>

static void kdbx_open_profile_set_error(char* error, size_t error_size, const char* message) {
    if(error != NULL && error_size > 0U) {
        snprintf(error, error_size, "%s", message != NULL ? message : "Invalid open profile.");
    }
}

static bool kdbx_open_profile_buffer_is_zero(const uint8_t* data, size_t data_size) {
    if(data == NULL) {
        return true;
    }

    for(size_t index = 0U; index < data_size; ++index) {
        if(data[index] != 0U) {
            return false;
        }
    }

    return true;
}

bool kdbx_open_profile_validate(const KDBXOpenProfile* profile, char* error, size_t error_size) {
    const bool is_aes =
        profile != NULL &&
        memcmp(profile->encryption_algorithm_uuid, KDBX_UUID_AES256, sizeof(KDBX_UUID_AES256)) ==
            0;
    const bool is_chacha20 = profile != NULL && memcmp(
                                                    profile->encryption_algorithm_uuid,
                                                    KDBX_UUID_CHACHA20,
                                                    sizeof(KDBX_UUID_CHACHA20)) == 0;

    if(profile == NULL) {
        kdbx_open_profile_set_error(error, error_size, "Open profile is unavailable.");
        return false;
    }
    if(!is_aes && !is_chacha20) {
        kdbx_open_profile_set_error(
            error, error_size, "Only AES256 or ChaCha20 KDBX 4 databases are supported.");
        return false;
    }
    if(profile->compression_algorithm != KDBX_COMPRESSION_NONE &&
       profile->compression_algorithm != KDBX_COMPRESSION_GZIP) {
        kdbx_open_profile_set_error(
            error, error_size, "Only raw or GZip-compressed KDBX 4 payloads are supported.");
        return false;
    }
    if(is_aes && profile->encryption_iv_size != 16U) {
        kdbx_open_profile_set_error(
            error, error_size, "AES-encrypted databases must use a 16-byte IV.");
        return false;
    }
    if(is_chacha20 && profile->encryption_iv_size != 12U) {
        kdbx_open_profile_set_error(
            error, error_size, "ChaCha20-encrypted databases must use a 12-byte nonce.");
        return false;
    }
    if(profile->payload_data_offset == 0U) {
        kdbx_open_profile_set_error(
            error, error_size, "Open profile payload offset is not initialized.");
        return false;
    }

    return true;
}

bool kdbx_open_profile_validate_for_stream(
    const KDBXOpenProfile* profile,
    char* error,
    size_t error_size) {
    if(!kdbx_open_profile_validate(profile, error, error_size)) {
        return false;
    }

    if(kdbx_open_profile_buffer_is_zero(profile->cipher_key, sizeof(profile->cipher_key))) {
        kdbx_open_profile_set_error(
            error, error_size, "The derived cipher key is missing from the open profile handoff.");
        return false;
    }

    if(kdbx_open_profile_buffer_is_zero(profile->hmac_key, sizeof(profile->hmac_key))) {
        kdbx_open_profile_set_error(
            error, error_size, "The derived HMAC key is missing from the open profile handoff.");
        return false;
    }

    return true;
}
