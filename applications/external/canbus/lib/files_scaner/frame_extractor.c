#include "files_scaner.h"
#include "frame_can.h"

#define STREAM_BUFFER_SIZE (32U)
#define FRAME_DELIMITER    (char)':'

#define TAG "FRAMES EXTRACTOR"

uint64_t stream_get_lines_count(Stream* stream) {
    uint64_t lines_count = 0;

    for(;;) {
        if(stream_seek_to_char(stream, '\n', StreamDirectionForward)) {
            lines_count++;
        } else {
            if(stream_tell(stream) < stream_size(stream) - 1) {
                lines_count++;
            }
            break;
        }
    }

    stream_rewind(stream);

    return lines_count;
}

bool stream_read_line_index(Stream* stream, FuriString* str_result, uint64_t line_index) {
    furi_string_reset(str_result);

    uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * STREAM_BUFFER_SIZE);

    uint64_t lines_count = 0;

    if(line_index < stream_get_lines_count(stream)) {
        do {
            bool error = false;
            bool result = false;
            uint16_t bytes_were_read = stream_read(stream, buffer, STREAM_BUFFER_SIZE);
            if(bytes_were_read == 0) {
                error = true;
                break;
            }

            for(uint16_t i = 0; i < bytes_were_read; i++) {
                if(buffer[i] == '\n') {
                    lines_count++;
                    if(!stream_seek(stream, i - bytes_were_read + 1, StreamOffsetFromCurrent)) {
                        error = true;
                        break;
                    }
                    if(lines_count - 1 == line_index) {
                        result = true;
                    } else {
                        furi_string_reset(str_result);
                    }
                    break;
                } else if(buffer[i] == '\r') {
                    // Ignore
                } else {
                    furi_string_push_back(str_result, buffer[i]);
                }
            }

            if(result || error) {
                break;
            }
        } while(true);
    }

    stream_rewind(stream);

    free(buffer);

    return furi_string_size(str_result) != 0;
}

char count_char(FuriString* frame_line, char target) {
    char count = 0;

    for(uint16_t i = 0; i < furi_string_size(frame_line); i++) {
        if(target == furi_string_get_char(frame_line, i)) count++;
    }

    return count;
}

void frame_splitter(FrameCAN* frame, FuriString* frame_line) {
    furi_string_trim(frame_line);

    char delimiter_count = count_char(frame_line, FRAME_DELIMITER);

    if(delimiter_count) {
        uint64_t delimiter_index_dir = furi_string_search_char(frame_line, FRAME_DELIMITER, 0);
        uint64_t delimiter_index_extended =
            furi_string_search_char(frame_line, FRAME_DELIMITER, delimiter_index_dir + 1);
        uint64_t delimiter_index_timestamp =
            furi_string_search_char(frame_line, FRAME_DELIMITER, delimiter_index_extended + 1);
        uint64_t delimiter_index_canid =
            furi_string_search_char(frame_line, FRAME_DELIMITER, delimiter_index_timestamp + 1);
        uint64_t delimiter_index_len =
            furi_string_search_char(frame_line, FRAME_DELIMITER, delimiter_index_canid + 1);

        FuriString* extended_str = furi_string_alloc();
        FuriString* timestamp_str = furi_string_alloc();
        FuriString* len_str = furi_string_alloc();

        furi_string_set(timestamp_str, frame_line);
        furi_string_set(extended_str, frame_line);
        furi_string_set(frame->dir, frame_line);
        furi_string_set(frame->can_id, frame_line);
        furi_string_set(len_str, frame_line);
        furi_string_set(frame->dlc, frame_line);

        furi_string_left(frame->dir, delimiter_index_dir);
        furi_string_mid(
            extended_str,
            delimiter_index_dir + 1,
            delimiter_index_extended - delimiter_index_dir - 1);
        furi_string_mid(
            timestamp_str,
            delimiter_index_extended + 1,
            delimiter_index_timestamp - delimiter_index_extended - 1);
        furi_string_mid(
            frame->can_id,
            delimiter_index_timestamp + 1,
            delimiter_index_canid - delimiter_index_timestamp - 1);
        furi_string_mid(
            len_str, delimiter_index_canid + 1, delimiter_index_len - delimiter_index_canid - 1);
        furi_string_mid(
            frame->dlc,
            delimiter_index_len + 1,
            furi_string_size(frame->dlc) - delimiter_index_len - 1);

        *frame->extended = (bool)atoi(furi_string_get_cstr(extended_str));
        *frame->timestamp = (uint16_t)atoi(furi_string_get_cstr(timestamp_str));
        *frame->len = furi_string_get_char(len_str, 0) - '0';

        furi_string_free(len_str);
        furi_string_free(timestamp_str);
        furi_string_free(extended_str);
    } else {
        FURI_LOG_E(TAG, "Error: can't read frame format");
    }
}

void frame_extractor(Storage* storage, const char* path, FrameCAN* frame, uint64_t index) {
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();

    if(file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(stream_read_line_index(stream, line, index)) {
            frame_splitter(frame, line);
        } else {
            FURI_LOG_E(TAG, "Failed to read line");
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open file");
    }

    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
}
