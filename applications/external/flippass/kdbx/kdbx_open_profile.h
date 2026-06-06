#pragma once

#include "kdbx_includes.h"

typedef struct {
    uint16_t version_minor;
    uint16_t version_major;
    uint8_t encryption_algorithm_uuid[16];
    uint32_t compression_algorithm;
    uint8_t encryption_iv[16];
    uint8_t encryption_iv_size;
    uint32_t payload_data_offset;
    uint64_t kdf_rounds;
    uint8_t kdf_salt[32];
    size_t kdf_salt_size;
    uint8_t composite_key[32];
    bool composite_key_ready;
    uint8_t transformed_key[32];
    bool transformed_key_ready;
    uint8_t cipher_key[32];
    uint8_t hmac_key[64];
} KDBXOpenProfile;

bool kdbx_open_profile_validate(const KDBXOpenProfile* profile, char* error, size_t error_size);
bool kdbx_open_profile_validate_for_stream(
    const KDBXOpenProfile* profile,
    char* error,
    size_t error_size);
