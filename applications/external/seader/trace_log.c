#include "trace_log.h"

#ifdef SEADER_ENABLE_TRACE_LOG

#include <storage/storage.h>
#include <toolbox/stream/buffered_file_stream.h>

static void seader_trace_write(FS_OpenMode open_mode, const char* line) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);

    Stream* stream = buffered_file_stream_alloc(storage);
    if(buffered_file_stream_open(stream, SEADER_TRACE_FILE_NAME, FSAM_READ_WRITE, open_mode)) {
        stream_seek(stream, 0, StreamOffsetFromEnd);
        stream_write(stream, (const uint8_t*)line, strlen(line));
    }

    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

void seader_trace_reset(void) {
    seader_trace_write(FSOM_CREATE_ALWAYS, "");
}

void seader_trace(const char* tag, const char* fmt, ...) {
    char message[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char line[224];
    snprintf(line, sizeof(line), "[%s] %s\n", tag, message);
    seader_trace_write(FSOM_OPEN_ALWAYS, line);
}

void seader_trace_hex(const char* tag, const char* prefix, const uint8_t* data, size_t len) {
    if(!data || len == 0) {
        seader_trace(tag, "%s <empty>", prefix);
        return;
    }

    /* Keep trace lines bounded; enough to diagnose short RF exchanges. */
    const size_t max_bytes = 32;
    size_t trace_len = len > max_bytes ? max_bytes : len;

    char hex[(max_bytes * 2) + 1];
    for(size_t i = 0; i < trace_len; i++) {
        snprintf(hex + (i * 2), sizeof(hex) - (i * 2), "%02x", data[i]);
    }
    hex[trace_len * 2] = '\0';

    if(trace_len < len) {
        seader_trace(tag, "%s len=%u data=%s...", prefix, (unsigned)len, hex);
    } else {
        seader_trace(tag, "%s len=%u data=%s", prefix, (unsigned)len, hex);
    }
}

#endif
