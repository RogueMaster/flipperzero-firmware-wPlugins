#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

extern uint8_t MP_FLIPPER_FILE_ACCESS_MODE_READ;
extern uint8_t MP_FLIPPER_FILE_ACCESS_MODE_WRITE;

extern uint8_t MP_FLIPPER_FILE_OPEN_MODE_OPEN_EXIST;
extern uint8_t MP_FLIPPER_FILE_OPEN_MODE_OPEN_ALWAYS;
extern uint8_t MP_FLIPPER_FILE_OPEN_MODE_OPEN_APPEND;
extern uint8_t MP_FLIPPER_FILE_OPEN_MODE_CREATE_NEW;
extern uint8_t MP_FLIPPER_FILE_OPEN_MODE_CREATE_ALWAYS;

void* mp_flipper_file_open(const char* name, uint8_t access_mode, uint8_t open_mode);
void* mp_flipper_file_new_file_descriptor(void* handle, const char* name, uint8_t access_mode, uint8_t open_mode, bool is_text);
bool mp_flipper_file_close(void* handle);
size_t mp_flipper_file_seek(void* handle, uint32_t offset);
size_t mp_flipper_file_tell(void* handle);
size_t mp_flipper_file_size(void* handle);
bool mp_flipper_file_sync(void* handle);
bool mp_flipper_file_eof(void* handle);
size_t mp_flipper_file_read(void* handle, void* buffer, size_t size, int* errcode);
size_t mp_flipper_file_write(void* handle, const void* buffer, size_t size, int* errcode);
