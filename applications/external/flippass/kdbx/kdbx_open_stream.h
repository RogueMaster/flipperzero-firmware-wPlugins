#pragma once

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
    KDBXOpenStreamCallback callback,
    void* context,
    char* error,
    size_t error_size);
