#include "SLCAN_serial.h"

#include <files_scaner.h>

#define FORMAT_WITH_TIMESTAMP    "%c%s%d%s%04X\r"
#define FORMAT_WITHOUT_TIMESTAMP "%c%s%d%s\r"

#define TAG "SLCAN WRITER"

void SLCAN_send_frame(FrameCAN* frame, bool* send_timestamp) {
    FuriString* SLCAN_frame = furi_string_alloc();

    furi_string_replace_all_str(frame->dlc, " ", "");

    if(*send_timestamp) {
        furi_string_printf(
            SLCAN_frame,
            FORMAT_WITH_TIMESTAMP,
            *frame->extended ? 'T' : 't',
            furi_string_get_cstr(frame->can_id),
            *frame->len,
            furi_string_get_cstr(frame->dlc),
            *frame->timestamp);
    } else {
        furi_string_printf(
            SLCAN_frame,
            FORMAT_WITHOUT_TIMESTAMP,
            *frame->extended ? 'T' : 't',
            furi_string_get_cstr(frame->can_id),
            *frame->len,
            furi_string_get_cstr(frame->dlc));
    }

    furi_hal_cdc_send(
        SLCAN_CDC_NUM, (uint8_t*)furi_string_get_cstr(SLCAN_frame), furi_string_size(SLCAN_frame));

    furi_string_free(SLCAN_frame);
}

void SLCAN_send_log(Storage* storage, FuriString* log_path, bool* send_timestamp) {
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    FrameCAN* frame = frame_can_alloc();

    if(file_stream_open(stream, furi_string_get_cstr(log_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line)) {
            frame_splitter(frame, line);
            SLCAN_send_frame(frame, send_timestamp);
        }

        file_stream_close(stream);
    }

    frame_can_free(frame);
    furi_string_free(line);
    stream_free(stream);
}
