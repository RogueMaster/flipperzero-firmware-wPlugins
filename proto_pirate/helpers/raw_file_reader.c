#include "raw_file_reader.h"

#ifdef ENABLE_SUB_DECODE_SCENE
#include <stdint.h>
#include <toolbox/stream/stream.h>
#include <lib/flipper_format/flipper_format.h>
#include "../protocols/protocols_common.h"

#define TAG "RawFileReader"

static const char local_flipper_format_delimiter = ':';
static const char local_flipper_format_comment = '#';
static const char local_flipper_format_eoln = '\n';
static const char local_flipper_format_eolr = '\r';

struct FlipperFormat {
    Stream* stream;
    bool strict_mode;
};

RawFileReader* raw_file_reader_alloc(void) {
    RawFileReader* reader = malloc(sizeof(RawFileReader));
    furi_check(reader);
    memset(reader, 0, sizeof(RawFileReader));
    return reader;
}

void raw_file_reader_free(RawFileReader* reader) {
    if(!reader) return;
    raw_file_reader_close(reader);
    free(reader);
}

static inline bool local_flipper_format_stream_is_space(char c) {
    return c == ' ' || c == '\t' || c == local_flipper_format_eolr;
}

static bool local_flipper_format_stream_read_value(Stream* stream, FuriString* value, bool* last) {
    enum {
        LeadingSpace,
        ReadValue,
        TrailingSpace
    } state = LeadingSpace;
    const size_t buffer_size = 32;
    uint8_t buffer[buffer_size];
    bool result = false;
    bool error = false;

    furi_string_reset(value);

    while(true) {
        size_t was_read = stream_read(stream, buffer, buffer_size);

        if(was_read == 0) {
            if(state != LeadingSpace && stream_eof(stream)) {
                result = true;
                *last = true;
            } else {
                error = true;
            }
        }

        for(size_t i = 0; i < was_read; i++) {
            const uint8_t data = buffer[i];

            if(state == LeadingSpace) {
                if(local_flipper_format_stream_is_space(data)) {
                    continue;
                } else if(data == local_flipper_format_eoln) {
                    stream_seek(stream, (int32_t)i - (int32_t)was_read, StreamOffsetFromCurrent);
                    error = true;
                    break;
                } else {
                    state = ReadValue;
                    furi_string_push_back(value, data);
                }
            } else if(state == ReadValue) {
                if(local_flipper_format_stream_is_space(data)) {
                    state = TrailingSpace;
                } else if(data == local_flipper_format_eoln) {
                    if(!stream_seek(
                           stream, (int32_t)i - (int32_t)was_read, StreamOffsetFromCurrent)) {
                        error = true;
                    } else {
                        result = true;
                        *last = true;
                    }
                    break;
                } else {
                    furi_string_push_back(value, data);
                }
            } else if(state == TrailingSpace) {
                if(local_flipper_format_stream_is_space(data)) {
                    continue;
                } else if(!stream_seek(
                              stream, (int32_t)i - (int32_t)was_read, StreamOffsetFromCurrent)) {
                    error = true;
                } else {
                    *last = (data == local_flipper_format_eoln);
                    result = true;
                }
                break;
            }
        }

        if(error || result) break;
    }

    return result;
}

static bool local_flipper_format_stream_read_valid_key(Stream* stream, FuriString* key) {
    furi_string_reset(key);
    const size_t buffer_size = 32;
    uint8_t buffer[buffer_size];

    bool found = false;
    bool error = false;
    bool accumulate = true;
    bool new_line = true;

    while(true) {
        size_t was_read = stream_read(stream, buffer, buffer_size);
        if(was_read == 0) break;

        for(size_t i = 0; i < was_read; i++) {
            uint8_t data = buffer[i];
            if(data == local_flipper_format_eoln) {
                // EOL found, clean data, start accumulating data and set the new_line flag
                furi_string_reset(key);
                accumulate = true;
                new_line = true;
            } else if(data == local_flipper_format_eolr) {
                // ignore
            } else if(data == local_flipper_format_comment && new_line) {
                // if there is a comment character and we are at the beginning of a new line
                // do not accumulate comment data and reset the new_line flag
                accumulate = false;
                new_line = false;
            } else if(data == local_flipper_format_delimiter) {
                if(new_line) {
                    // we are on a "new line" and found the delimiter
                    // this can only be if we have previously found some kind of key, so
                    // clear the data, set the flag that we no longer want to accumulate data
                    // and reset the new_line flag
                    furi_string_reset(key);
                    accumulate = false;
                    new_line = false;
                } else {
                    // parse the delimiter only if we are accumulating data
                    if(accumulate) {
                        // we found the delimiter, move the rw pointer to the delimiter location
                        // and signal that we have found something
                        if(!stream_seek(
                               stream, (int32_t)i - (int32_t)was_read, StreamOffsetFromCurrent)) {
                            error = true;
                            break;
                        }

                        found = true;
                        break;
                    }
                }
            } else {
                // just new symbol, reset the new_line flag
                new_line = false;
                if(accumulate) {
                    // and accumulate data if we want
                    furi_string_push_back(key, data);
                }
            }
        }

        if(found || error) break;
    }

    return found;
}

static bool
    local_flipper_format_stream_seek_to_key(Stream* stream, const char* key, bool strict_mode) {
    bool found = false;
    FuriString* read_key;

    read_key = furi_string_alloc();

    while(!stream_eof(stream)) {
        if(local_flipper_format_stream_read_valid_key(stream, read_key)) {
            if(furi_string_cmp_str(read_key, key) == 0) {
                if(!stream_seek(stream, 2, StreamOffsetFromCurrent)) break;

                found = true;
                break;
            } else if(strict_mode) {
                found = false;
                break;
            }
        }
    }
    furi_string_free(read_key);

    return found;
}

static bool local_flipper_format_stream_get_value_count(
    Stream* stream,
    const char* key,
    uint32_t* count,
    bool strict_mode) {
    bool result = false;
    bool last = false;

    FuriString* value;
    value = furi_string_alloc();

    do {
        if(!local_flipper_format_stream_seek_to_key(stream, key, strict_mode)) break;
        *count = 0;

        result = true;
        while(true) {
            if(!local_flipper_format_stream_read_value(stream, value, &last)) {
                result = false;
                break;
            }

            *count = *count + 1;
            if(last) break;
        }

    } while(false);

    furi_string_free(value);
    return result;
}

bool raw_file_reader_open(RawFileReader* reader, const char* file_path) {
    if(!reader || !file_path) return false;

    raw_file_reader_close(reader);

    reader->storage = furi_record_open(RECORD_STORAGE);
    reader->storage_opened = true;
    reader->ff = flipper_format_file_alloc(reader->storage);

    if(!flipper_format_file_open_existing(reader->ff, file_path)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", file_path);
        raw_file_reader_close(reader);
        return false;
    }

    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;

    bool valid = false;
    do {
        if(!flipper_format_read_header(reader->ff, temp_str, &version)) {
            FURI_LOG_E(TAG, "Failed to read header");
            break;
        }

        if(furi_string_cmp_str(temp_str, "Flipper SubGhz RAW File") != 0) {
            FURI_LOG_E(TAG, "Not a RAW file");
            break;
        }

        if(!flipper_format_read_string(reader->ff, FF_PROTOCOL, temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol field");
            break;
        }

        if(furi_string_cmp_str(temp_str, "RAW") != 0) {
            FURI_LOG_E(TAG, "Protocol is not RAW");
            break;
        }

        valid = true;
    } while(false);

    furi_string_free(temp_str);

    if(!valid) {
        raw_file_reader_close(reader);
        return false;
    }

    reader->buffer_count = 0;
    reader->buffer_index = 0;
    reader->file_finished = false;
    reader->current_level = true;

    FURI_LOG_I(TAG, "Opened RAW file: %s", file_path);

    reader->count = 0;
    uint32_t temp_count = 0;

    while(local_flipper_format_stream_get_value_count(
        reader->ff->stream, "RAW_Data", &temp_count, reader->ff->strict_mode)) {
        //reader->file_finished = true;
        reader->count += temp_count;
    }
    flipper_format_rewind(reader->ff);

    return true;
}

void raw_file_reader_close(RawFileReader* reader) {
    if(!reader) return;

    if(reader->ff) {
        flipper_format_free(reader->ff);
        reader->ff = NULL;
    }

    if(reader->storage_opened) {
        furi_record_close(RECORD_STORAGE);
        reader->storage_opened = false;
    }

    reader->storage = NULL;
    reader->buffer_count = 0;
    reader->buffer_index = 0;
    reader->count = 0;
    reader->file_finished = false;
}

static bool raw_file_reader_load_chunk(RawFileReader* reader) {
    if(reader->file_finished) return false;

    size_t to_read = (reader->count < RAW_READER_BUFFER_SIZE) ? reader->count :
                                                                RAW_READER_BUFFER_SIZE;

    if(!flipper_format_read_int32(reader->ff, "RAW_Data", reader->buffer, to_read)) {
        reader->file_finished = true;
        return false;
    }

    reader->buffer_count = to_read;
    reader->buffer_index = 0;
    reader->count -= to_read;

    return true;
}

bool raw_file_reader_get_next(RawFileReader* reader, bool* level, uint32_t* duration) {
    if(!reader || !level || !duration) return false;

    if(memmgr_get_free_heap() < 1024) {
        FURI_LOG_E(TAG, "Not enough memory to continue reading");
        return false;
    }

    if(reader->buffer_index >= reader->buffer_count) {
        if(!raw_file_reader_load_chunk(reader)) {
            return false;
        }
    }

    int32_t value = reader->buffer[reader->buffer_index++];

    if(value >= 0) {
        *level = true;
        *duration = (uint32_t)value;
    } else {
        *level = false;
        *duration = (uint32_t)(-value);
    }

    return true;
}

bool raw_file_reader_is_finished(RawFileReader* reader) {
    if(!reader) return true;
    return reader->file_finished && (reader->buffer_index >= reader->buffer_count);
}
#endif // ENABLE_SUB_DECODE_SCENE
