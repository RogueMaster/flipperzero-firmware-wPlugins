#pragma once

#include "../protopirate_app_i.h"
#ifdef ENABLE_SUB_DECODE_SCENE
#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define RAW_READER_BUFFER_SIZE 512

typedef struct {
    Storage* storage;
    FlipperFormat* ff;
    int32_t buffer[RAW_READER_BUFFER_SIZE];
    size_t buffer_count;
    size_t buffer_index;
    uint32_t count;
    bool file_finished;
    bool current_level;
    bool storage_opened;
} RawFileReader;

RawFileReader* raw_file_reader_alloc(void);
void raw_file_reader_free(RawFileReader* reader);
bool raw_file_reader_open(RawFileReader* reader, const char* file_path);
void raw_file_reader_close(RawFileReader* reader);
bool raw_file_reader_get_next(RawFileReader* reader, bool* level, uint32_t* duration);
bool raw_file_reader_is_finished(RawFileReader* reader);
#endif // ENABLE_SUB_DECODE_SCENE
