#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#include <frame_can.h>

typedef struct {
    FuriString* path;
    uint64_t file_index;
    uint64_t frames_count;
    uint64_t frame_index;
} FileActive;

FileActive* file_active_alloc(void);
void file_active_free(FileActive* file_active);

uint64_t storage_dir_get_files_count(Storage* storage, const char* path);

bool storage_dir_read_index(
    Storage* storage,
    const char* path,
    FuriString* file_path,
    uint64_t index);

void frame_extractor(Storage* storage, const char* path, FrameCAN* frame, uint64_t index);

uint64_t stream_get_lines_count(Stream* stream);

void frame_splitter(FrameCAN* frame, FuriString* frame_line);
