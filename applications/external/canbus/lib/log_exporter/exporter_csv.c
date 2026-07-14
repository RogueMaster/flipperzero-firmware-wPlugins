#include "log_exporter.h"

#include <files_scaner.h>

#define PATHEXPORTS APP_DATA_PATH("exports")
#define PATHLOGS    APP_DATA_PATH("logs")

#define CSV_HEADER       "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
#define CSV_FORMAT_FRAME "%d,%s,%s,%s,0,%d,%s,\n"
#define DIR_RX           "Rx"
#define DIR_TX           "Tx"

#define TAG "LOG EXPORTER"

void csv_frame_format(FrameCAN* frame, FuriString* csv_format) {
    furi_string_reset(csv_format);

    furi_string_replace_all_str(frame->dlc, " ", ",");

    furi_string_printf(
        csv_format,
        CSV_FORMAT_FRAME,
        *frame->timestamp,
        furi_string_get_cstr(frame->can_id),
        *frame->extended ? "true" : "false",
        !furi_string_cmp_str(frame->dir, "r") ? DIR_RX :
        !furi_string_cmp_str(frame->dir, "t") ? DIR_TX :
                                                DIR_RX,
        (furi_string_size(frame->dlc) + 1) / 3,
        furi_string_get_cstr(frame->dlc));
}

void export_log_as_csv(Storage* storage, FuriString* log_path, FuriString* out_path) {
    Stream* stream = file_stream_alloc(storage);
    Stream* stream_out = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    FrameCAN* frame = frame_can_alloc();

    furi_string_set(out_path, log_path);
    furi_string_replace_all_str(out_path, PATHLOGS, PATHEXPORTS);
    furi_string_left(out_path, furi_string_search_char(out_path, '.', 0));
    furi_string_cat_str(out_path, ".csv");

    if(file_stream_open(stream, furi_string_get_cstr(log_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        file_stream_open(stream_out, furi_string_get_cstr(out_path), FSAM_WRITE, FSOM_OPEN_ALWAYS);

        stream_write_cstring(stream_out, CSV_HEADER);

        while(stream_read_line(stream, line)) {
            frame_splitter(frame, line);
            furi_string_reset(line);
            csv_frame_format(frame, line);
            stream_write_string(stream_out, line);
        }

        file_stream_close(stream_out);
        file_stream_close(stream);
    }

    frame_can_free(frame);
    furi_string_free(line);
    stream_free(stream_out);
    stream_free(stream);
}
