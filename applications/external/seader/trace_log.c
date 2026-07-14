#include "trace_log.h"

#if SEADER_VERBOSE_LOG || defined(SEADER_ENABLE_TRACE_LOG)
static bool seader_hex_format_truncated(
    char* hex,
    size_t hex_size,
    const uint8_t* data,
    size_t len,
    size_t max_bytes) {
    if(!hex || hex_size == 0U) {
        return false;
    }

    if(!data || len == 0U) {
        hex[0] = '\0';
        return false;
    }

    const size_t display_len = len > max_bytes ? max_bytes : len;
    for(size_t i = 0; i < display_len; i++) {
        snprintf(hex + (i * 2U), hex_size - (i * 2U), "%02x", data[i]);
    }
    hex[display_len * 2U] = '\0';
    return display_len < len;
}
#endif

#ifdef SEADER_ENABLE_TRACE_LOG
#include <storage/storage.h>
#include <toolbox/stream/buffered_file_stream.h>
#endif

void seader_verbose_log(FuriLogLevel level, const char* tag, const char* fmt, ...) {
#if SEADER_VERBOSE_LOG
    va_list args;
    va_start(args, fmt);
    char message[192];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    furi_log_print_format(level, tag, "%s", message);
#else
    (void)level;
    (void)tag;
    (void)fmt;
#endif
}

void seader_verbose_hex(
    FuriLogLevel level,
    const char* tag,
    const char* prefix,
    const uint8_t* data,
    size_t len) {
#if SEADER_VERBOSE_LOG
    if(!data || len == 0U) {
        furi_log_print_format(level, tag, "%s: <empty>", prefix);
        return;
    }

    char hex[(32U * 2U) + 1U];
    const bool truncated = seader_hex_format_truncated(hex, sizeof(hex), data, len, 32U);
    if(truncated) {
        furi_log_print_format(level, tag, "%s len=%u: %s...", prefix, (unsigned)len, hex);
    } else {
        furi_log_print_format(level, tag, "%s len=%u: %s", prefix, (unsigned)len, hex);
    }
#else
    (void)level;
    (void)tag;
    (void)prefix;
    (void)data;
    (void)len;
#endif
}

#ifdef SEADER_ENABLE_TRACE_LOG

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

    char hex[(32U * 2U) + 1U];
    if(seader_hex_format_truncated(hex, sizeof(hex), data, len, 32U)) {
        seader_trace(tag, "%s len=%u data=%s...", prefix, (unsigned)len, hex);
    } else {
        seader_trace(tag, "%s len=%u data=%s", prefix, (unsigned)len, hex);
    }
}

#endif
