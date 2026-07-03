#pragma once

#include "../protopirate_app_i.h"
#ifdef ENABLE_SUB_DECODE_SCENE
#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <toolbox/stream/stream.h>

#define RAW_READER_BUFFER_SIZE 2048U

typedef struct {
    Storage* storage;
    FlipperFormat* ff;
    Stream* stream;
    uint8_t buffer[RAW_READER_BUFFER_SIZE];
    uint16_t buffer_count;
    uint16_t buffer_index;
    bool in_line;
    bool file_finished;
    bool storage_opened;
    uint64_t file_size;
    uint64_t data_start_offset;
    uint64_t stream_pos;
} RawFileReader;

RawFileReader* raw_file_reader_alloc(void);
void raw_file_reader_free(RawFileReader* reader);
bool raw_file_reader_open(RawFileReader* reader, const char* file_path);
void raw_file_reader_close(RawFileReader* reader);
bool raw_file_reader_get_next(RawFileReader* reader, bool* level, uint32_t* duration);
bool raw_file_reader_is_finished(RawFileReader* reader);
uint8_t raw_file_reader_get_progress(const RawFileReader* reader);
#endif // ENABLE_SUB_DECODE_SCENE
