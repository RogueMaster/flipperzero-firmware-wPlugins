#include "lawicel_serial.h"

#include <files_scaner.h>

#define FORMAT_WITH_TIMESTAMP    "%c%s%d%s%04X\r"
#define FORMAT_WITHOUT_TIMESTAMP "%c%s%d%s\r"

#define TAG "LAWICEL WRITER"

void lawicel_send_frame(FrameCAN* frame, bool* send_timestamp) {
    FuriString* lawicel_frame = furi_string_alloc();

    furi_string_replace_all_str(frame->dlc, " ", "");

    if(*send_timestamp) {
        furi_string_printf(
            lawicel_frame,
            FORMAT_WITH_TIMESTAMP,
            *frame->extended ? 'T' : 't',
            furi_string_get_cstr(frame->can_id),
            *frame->len,
            furi_string_get_cstr(frame->dlc),
            *frame->timestamp);
    } else {
        furi_string_printf(
            lawicel_frame,
            FORMAT_WITHOUT_TIMESTAMP,
            *frame->extended ? 'T' : 't',
            furi_string_get_cstr(frame->can_id),
            *frame->len,
            furi_string_get_cstr(frame->dlc));
    }

    furi_hal_cdc_send(
        LAWICEL_CDC_NUM,
        (uint8_t*)furi_string_get_cstr(lawicel_frame),
        furi_string_size(lawicel_frame));

    furi_string_free(lawicel_frame);
}

void lawicel_send_log(Storage* storage, FuriString* log_path, bool* send_timestamp) {
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    FrameCAN* frame = frame_can_alloc();

    if(file_stream_open(stream, furi_string_get_cstr(log_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line)) {
            frame_splitter(frame, line);
            lawicel_send_frame(frame, send_timestamp);
        }

        file_stream_close(stream);
    }

    frame_can_free(frame);
    furi_string_free(line);
    stream_free(stream);
}
