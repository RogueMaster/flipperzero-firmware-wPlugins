#pragma once

#include "../flippass_crypto.h"
#include "kdbx_open_profile.h"

typedef bool (*KDBXOpenStreamCallback)(const uint8_t* data, size_t data_size, void* context);

bool kdbx_open_stream_payload(
    const char* file_path,
    const KDBXOpenProfile* profile,
    KDBXOpenStreamCallback callback,
    void* context,
    char* error,
    size_t error_size);

bool kdbx_open_stream_outer_payload(
    const char* file_path,
    const KDBXOpenProfile* profile,
    const FlipPassCryptoApiV1* crypto,
    bool (*validate_profile)(const KDBXOpenProfile* profile, char* error, size_t error_size),
    KDBXOpenStreamCallback callback,
    void* context,
    char* error,
    size_t error_size);
