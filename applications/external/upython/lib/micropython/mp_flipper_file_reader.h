#pragma once

#include <stddef.h>
#include <stdint.h>

#define MP_FLIPPER_FILE_READER_EOF ((uint32_t)(-1))

void* mp_flipper_file_reader_context_alloc(const char* filename);

uint32_t mp_flipper_file_reader_read(void* data);

void mp_flipper_file_reader_close(void* data);
